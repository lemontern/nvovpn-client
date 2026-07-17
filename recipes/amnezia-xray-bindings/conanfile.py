from conan import ConanFile
from conan.tools.files import get, copy, collect_libs, chdir, rename
from conan.tools.layout import basic_layout
from conan.errors import ConanInvalidConfiguration
from conan.tools.gnu import Autotools, AutotoolsToolchain

import os

class AmneziaXrayBindings(ConanFile):
    name = "amnezia-xray-bindings"
    version = "1.1.0"
    settings = "os", "arch", "compiler"

    @property
    def _goos(self):
        return {
            "Linux": "linux",
            "iOS": "ios",
            "Macos": "darwin",
            "Windows": "windows"
        }.get(str(self.settings.os))
    
    # Одиночная conan-арка → GOARCH. Универсальную арку ("armv8|x86_64") НЕ маппим
    # напрямую — она разбивается в _archs и собирается послайсно (см. build()).
    @staticmethod
    def _goarch_of(conan_arch):
        return {
            "x86": "386",
            "x86_64": "amd64",
            "armv8": "arm64"
        }.get(conan_arch)

    # clang-арка для CGO при кросс-сборке слайсы (GOARCH!=арка раннера).
    @staticmethod
    def _clang_arch_of(conan_arch):
        return {
            "x86": "i386",
            "x86_64": "x86_64",
            "armv8": "arm64"
        }.get(conan_arch)

    @property
    def _archs(self):
        # macOS NE собирается универсальным бинарём → settings.arch == "armv8|x86_64".
        return str(self.settings.arch).split("|")

    @property
    def _is_universal(self):
        return len(self._archs) > 1

    @property
    def _goarch(self):
        # Обратная совместимость для одноарочного пути (Linux/Windows/один macOS).
        return self._goarch_of(self._archs[0]) if not self._is_universal else None

    @property
    def _is_windows(self):
        return str(self.settings.os).startswith("Windows")

    def config_options(self):
        self.package_type = "shared-library" if self._is_windows else "static-library"

    def configure(self):
        self.settings.rm_safe("compiler.libcxx")
        self.settings.rm_safe("compiler.cppstd")
        if self._is_windows:
            # mingw-builds is being used on Windows
            del self.settings.compiler

    def layout(self):
        basic_layout(self)

    def build_requirements(self):
        self.tool_requires("go/1.26.0")
        if self._is_windows:
            self.win_bash = True
            if not self.conf.get("tools.microsoft.bash:path", check_type=str):
                self.tool_requires("msys2/cci.latest")
            self.tool_requires("mingw-builds/15.1.0")

    def validate(self):
        if not self._goos or any(self._goarch_of(a) is None for a in self._archs):
            raise ConanInvalidConfiguration(
                f"{self.name} v{self.version} does not support {self.settings.os} {self.settings.arch}"
            )

    def source(self):
        get(self, "https://github.com/amnezia-vpn/amnezia-xray-bindings/archive/v1.1.0.zip",
            sha256="6ea768ec7002cedd422a39aea17704b888acaf794432aa5937cfc92fb6d80eb5", strip_root=True)

    def generate(self):
        tc = AutotoolsToolchain(self)
        tc.make_args = [
            "LIB_ARC=libamnezia_xray.a"
        ]
        env = tc.environment()
        env.define("GOOS", self._goos)
        if self._is_universal:
            # Универсальный macOS: combined-арочные флаги тулчейна (-arch arm64 -arch x86_64)
            # ломают одноарочный `go build -buildmode=c-archive`. Env для каждой слайсы
            # задаётся вручную в build(); здесь combined GOARCH/CGO намеренно НЕ определяем.
            pass
        else:
            env.define("ARCH", self._goarch)
            env.define("GOARCH", self._goarch)
            env.define("CGO_LDFLAGS", tc.ldflags)
            env.define("CGO_CFLAGS", tc.cflags)
        if self._is_windows:
            env.define("OS", "windows")
        tc.generate(env)

    def build(self):
        with chdir(self, self.source_folder):
            if not self._is_universal:
                # Одноарочный путь (Linux/Windows/один macOS) — как было.
                Autotools(self).make()
                return

            # NvoVPN: универсальный macOS (armv8|x86_64). Upstream Makefile одноарочный
            # (GOARCH=$(ARCH) go build -buildmode=c-archive), поэтому собираем каждую слайсу
            # отдельно в свой BUILD_DIR и склеиваем через lipo. CGO_CFLAGS/CGO_LDFLAGS
            # передаём ЧЕРЕЗ ОКРУЖЕНИЕ (префикс), чтобы Makefile до-добавил `-isysroot`
            # (правило `CGO_CFLAGS += -isysroot ...` при OS=macos срабатывает только для
            # env-переменной, не для make-аргумента). OS/ARCH/BUILD_DIR/LIB_ARC — make-аргументы.
            slices = []
            for conan_arch in self._archs:
                goarch = self._goarch_of(conan_arch)      # arm64 / amd64
                clang_arch = self._clang_arch_of(conan_arch)  # arm64 / x86_64
                bdir = f"build_{goarch}"
                self.run(
                    f'CGO_CFLAGS="-arch {clang_arch}" CGO_LDFLAGS="-arch {clang_arch}" '
                    f'make OS=macos ARCH={goarch} BUILD_DIR={bdir} LIB_ARC=libamnezia_xray.a'
                )
                slices.append(f"{bdir}/libamnezia_xray.a")

            self.run("mkdir -p build")
            if len(slices) > 1:
                self.run(f'lipo -create {" ".join(slices)} -output build/libamnezia_xray.a')
            else:
                self.run(f'cp {slices[0]} build/libamnezia_xray.a')
            # Заголовок c-archive одинаков для всех арок — берём из первой слайсы.
            first_bdir = f"build_{self._goarch_of(self._archs[0])}"
            self.run(f'cp {first_bdir}/libamnezia_xray.h build/libamnezia_xray.h')
            # Убираем арочные слайсы, чтобы package() (рекурсивный glob *.a) не упаковал
            # их как отдельные библиотеки — иначе collect_libs вернёт дубликаты.
            for conan_arch in self._archs:
                self.run(f'rm -rf build_{self._goarch_of(conan_arch)}')

    def _rename_header(self):
        if not self._is_windows:
            rename(self, os.path.join(self.package_folder, "include", "libamnezia_xray.h"),
                    os.path.join(self.package_folder, "include", "amnezia_xray.h"))

    def _rename_libs(self):
        # workaround of bad naming strategy in amnezia-xray-bindings
        # TODO: change it and kick out the code below
        lib_dir = os.path.join(self.package_folder, "lib")
        for fname in os.listdir(lib_dir):
            if not fname.startswith("lib"):
                src = os.path.join(lib_dir, fname)
                dst = os.path.join(lib_dir, "lib" + fname)
                os.rename(src, dst)

    def package(self):
        copy(self, "*.h", src=self.build_folder, dst=os.path.join(self.package_folder, "include"))
        copy(self, "*.a", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"))
        copy(self, "*.lib", src=self.build_folder, dst=os.path.join(self.package_folder, "lib"))
        copy(self, "*.dll", src=self.build_folder, dst=os.path.join(self.package_folder, "bin"))
        self._rename_header()

    def package_info(self):
        self.cpp_info.set_property("cmake_target_name", "amnezia::xray-bindings")
        self.cpp_info.libs = collect_libs(self)
