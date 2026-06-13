sc stop AmneziaWGTunnel$AmneziaVPN
sc delete AmneziaWGTunnel$AmneziaVPN
taskkill /IM "NvoVPN-service.exe" /F
taskkill /IM "NvoVPN.exe" /F
exit /b 0
