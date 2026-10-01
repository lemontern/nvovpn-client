import Foundation
import StoreKit

@available(iOS 15.0, macOS 12.0, *)
@objcMembers
public class StoreKit2Helper: NSObject {

    public static let shared = StoreKit2Helper()
    private static let errorDomain = "StoreKit2Helper"

    private struct EntitlementInfo {
        let transactionId: UInt64
        let originalTransactionId: UInt64
        let productId: String
        let purchaseDate: Date

        var dictionary: NSDictionary {
            [
                "transactionId": String(transactionId),
                "originalTransactionId": String(originalTransactionId),
                "productId": productId
            ]
        }
    }

    public func fetchCurrentEntitlements(completion: @escaping (Bool, [NSDictionary]?, NSError?) -> Void) {
        Task { @MainActor in
            do {
                try await AppStore.sync()

                var entitlements: [EntitlementInfo] = []
                for await result in Transaction.currentEntitlements {
                    switch result {
                    case .verified(let transaction):
                        entitlements.append(EntitlementInfo(transactionId: transaction.id,
                                                            originalTransactionId: transaction.originalID,
                                                            productId: transaction.productID,
                                                            purchaseDate: transaction.purchaseDate))
                    case .unverified(_, let error):
                        print("[IAP][StoreKit2] Unverified transaction skipped: \(error.localizedDescription)")
                    }
                }
                let sortedEntitlements = entitlements.sorted { lhs, rhs in
                    if lhs.purchaseDate != rhs.purchaseDate {
                        return lhs.purchaseDate > rhs.purchaseDate
                    }
                    return lhs.transactionId > rhs.transactionId
                }.map { $0.dictionary }
                completion(true, sortedEntitlements, nil)
            } catch {
                completion(false, nil, error as NSError)
            }
        }
    }

    public func purchaseProduct(productIdentifier: String, completion: @escaping (Bool, String?, String?, String?, NSError?) -> Void) {
        Task {
            do {
                let products = try await Product.products(for: [productIdentifier])
                guard let product = products.first else {
                    completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                     error: makeError(code: 0, description: "Product not found"))
                    return
                }
                let result = try await product.purchase()
                switch result {
                case .success(let verification):
                    switch verification {
                    case .verified(let transaction):
                        // 01.10.2026 (аудит M-2): транзакцию НЕ завершаем здесь. finish() — только после того, как наш
                        // сервер принял чек (finishTransaction ниже). Иначе при сетевом сбое деньги списаны, а подписка
                        // не активирована, и StoreKit больше о покупке не напомнит (так потерялась покупка 20.09.2026).
                        completePurchase(completion: completion, success: true, transactionId: String(transaction.id),
                                         productId: transaction.productID, originalTransactionId: String(transaction.originalID), error: nil)
                    case .unverified(_, let error):
                        completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                         error: error as NSError)
                    }
                case .userCancelled:
                    completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                     error: makeError(code: 1, description: "Purchase cancelled"))
                case .pending:
                    completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                     error: makeError(code: 2, description: "Purchase pending"))
                @unknown default:
                    completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                     error: makeError(code: 3, description: "Unknown purchase result"))
                }
            } catch {
                completePurchase(completion: completion, success: false, transactionId: nil, productId: nil, originalTransactionId: nil,
                                 error: error as NSError)
            }
        }
    }

    /// 01.10.2026: завершить транзакцию после подтверждения сервером. Ищем среди незавершённых.
    public func finishTransaction(transactionId: String, completion: @escaping (Bool) -> Void) {
        Task {
            for await result in Transaction.unfinished {
                if case .verified(let transaction) = result, String(transaction.id) == transactionId {
                    await transaction.finish()
                    print("[IAP][StoreKit2] finished transaction \(transactionId)")
                    DispatchQueue.main.async { completion(true) }
                    return
                }
            }
            DispatchQueue.main.async { completion(false) }
        }
    }

    /// 01.10.2026: незавершённые транзакции (покупка без ответа сервера, продление, отложенная покупка) —
    /// приложение досылает их на сервер при запуске и после входа.
    public func fetchUnfinishedTransactions(completion: @escaping ([NSDictionary]) -> Void) {
        Task {
            var list: [NSDictionary] = []
            for await result in Transaction.unfinished {
                if case .verified(let transaction) = result {
                    list.append(EntitlementInfo(transactionId: transaction.id,
                                                originalTransactionId: transaction.originalID,
                                                productId: transaction.productID,
                                                purchaseDate: transaction.purchaseDate).dictionary)
                }
            }
            DispatchQueue.main.async { completion(list) }
        }
    }

    private var updatesTask: Task<Void, Never>?

    /// 01.10.2026: слушатель Transaction.updates — продления, покупки «Ask to Buy», транзакции с других устройств.
    /// Раньше его не было, и такие транзакции некуда было доставить.
    public func startTransactionUpdates(handler: @escaping (NSDictionary) -> Void) {
        if updatesTask != nil {
            return
        }
        updatesTask = Task.detached {
            for await result in Transaction.updates {
                if case .verified(let transaction) = result {
                    let info = EntitlementInfo(transactionId: transaction.id,
                                               originalTransactionId: transaction.originalID,
                                               productId: transaction.productID,
                                               purchaseDate: transaction.purchaseDate).dictionary
                    DispatchQueue.main.async { handler(info) }
                }
            }
        }
    }

    private func storefrontCurrencyCode(for product: Product) -> String {
        product.priceFormatStyle.locale.currencyCode ?? ""
    }

    private func subscriptionBillingMonths(_ period: Product.SubscriptionPeriod) -> Double {
        let periodValue = Double(period.value)
        switch period.unit {
        case .day:
            return periodValue / 30.0
        case .week:
            return periodValue * 7.0 / 30.0
        case .month:
            return periodValue
        case .year:
            return periodValue * 12.0
        @unknown default:
            return periodValue
        }
    }

    public func fetchProducts(identifiers: Set<String>, completion: @escaping ([NSDictionary], [String], NSError?) -> Void) {
        Task {
            do {
                let products = try await Product.products(for: identifiers)
                let productDicts = products.map { product in productDictionary(for: product) }
                let fetchedIds = Set(products.map { $0.id })
                let invalidIdentifiers = identifiers.filter { !fetchedIds.contains($0) }
                DispatchQueue.main.async { completion(productDicts, Array(invalidIdentifiers), nil) }
            } catch {
                DispatchQueue.main.async { completion([], Array(identifiers), error as NSError) }
            }
        }
    }

    private func makeError(code: Int, description: String) -> NSError {
        NSError(domain: Self.errorDomain, code: code, userInfo: [NSLocalizedDescriptionKey: description])
    }

    private func completePurchase(completion: @escaping (Bool, String?, String?, String?, NSError?) -> Void,
                                  success: Bool,
                                  transactionId: String?,
                                  productId: String?,
                                  originalTransactionId: String?,
                                  error: NSError?) {
        DispatchQueue.main.async {
            completion(success, transactionId, productId, originalTransactionId, error)
        }
    }

    private func productDictionary(for product: Product) -> NSDictionary {
        let currencyCode = storefrontCurrencyCode(for: product)
        var productData: [String: Any] = [
            "productId": product.id,
            "title": product.displayName,
            "description": product.description,
            "price": "\(product.price)",
            "displayPrice": product.displayPrice,
            "currencyCode": currencyCode,
            "priceAmount": NSDecimalNumber(decimal: product.price).doubleValue
        ]
        if let subscription = product.subscription {
            let billingMonths = subscriptionBillingMonths(subscription.subscriptionPeriod)
            productData["subscriptionBillingMonths"] = billingMonths
            if let perMonthPrice = displayPricePerMonth(for: product, billingMonths: billingMonths, currencyCode: currencyCode) {
                productData["displayPricePerMonth"] = perMonthPrice
            }
        }
        return productData as NSDictionary
    }

    private func displayPricePerMonth(for product: Product, billingMonths: Double, currencyCode: String) -> String? {
        if billingMonths <= 1e-6 {
            return nil
        }

        let perMonthPrice = product.price / Decimal(billingMonths)
        let formatter = NumberFormatter()
        formatter.numberStyle = .currency
        formatter.locale = product.priceFormatStyle.locale
        if !currencyCode.isEmpty {
            formatter.currencyCode = currencyCode
        }
        return formatter.string(from: NSDecimalNumber(decimal: perMonthPrice))
    }
}
