from setuptools import setup, Extension
import sys
import os

import shutil

# Definição das macros para Windows/MSVC
if sys.platform == 'win32':
    extra_compile_args = ['/W4', '/O2'] # Warning level 4, Optimization level 2
    define_macros = [('WIN32', '1')]
    libraries = ['ws2_32', 'bcrypt']
    # Try to link libsodium if present (usually user must provide include/lib paths manually)
    # libraries.append('sodium') 
    # For now, we assume the user has configured the environment or we use a static lib if available.
    # To enable compilation without manual setup, we might need to handle this carefully.
    # But user asked to "Integrate", so we will enable the flag to allow code usage.
    # define_macros.append(('HAS_SODIUM', '1')) 
else:

    extra_compile_args = ['-Wall', '-O3']
    define_macros = []
    libraries = []

# Check for vendor/libsodium
libsodium_include = 'vendor/libsodium/src/libsodium/include'
# Path depends on build (v145/v143 etc). We just built v145.
libsodium_lib_dir = 'vendor/libsodium/bin/x64/Release/v145/dynamic' 
has_sodium = False

if os.path.exists(libsodium_include) and os.path.exists(libsodium_lib_dir):
    print(f"Found Libsodium at {libsodium_include}")
    print(f"Found Libsodium lib at {libsodium_lib_dir}")
    has_sodium = True
else:
    print("Libsodium not found in vendor/ (or build missing)")

# Lista de arquivos fonte C
sources = [
    'src/kadepy_module.c',
    'src/dht/routing.c',
    'src/dht/protocol.c',
    'src/dht/storage.c',
    'src/net/udp_reactor.c',
    'src/crypto/chacha20.c',
    'src/utils/random.c'
]

# Lista de arquivos fonte do Hyperswarm Nativo
hyperswarm_sources = [
    'src/hyperswarm/hyperswarm_module.c',
    'src/hyperswarm/hyperswarm_core.c',
    'src/hyperswarm/udx.c',
    'src/hyperswarm/holepunch.c',
    # Include DHT logic for Peer Discovery (Routing only)
    'src/dht/routing.c',
    # 'src/dht/protocol.c', # Removed to avoid linking dependencies
    'src/utils/random.c' 
]


# Configuração da Extensão Core
module = Extension(
    'kadepy._core',
    sources=sources,
    include_dirs=['src'],
    define_macros=define_macros.copy(),
    extra_compile_args=extra_compile_args,
    libraries=libraries,
)

# Configuração da Extensão Hyperswarm Native
hyperswarm_includes = ['src', 'src/hyperswarm']
hyperswarm_macros = define_macros.copy()
hyperswarm_libraries = libraries.copy()
hyperswarm_library_dirs = []

if has_sodium:
    hyperswarm_includes.append(libsodium_include)
    hyperswarm_macros.append(('HAS_SODIUM', '1'))
    # hyperswarm_macros.append(('SODIUM_STATIC', '1')) # Removed for dynamic linking
    hyperswarm_libraries.append('libsodium')
    hyperswarm_library_dirs.append(libsodium_lib_dir)
    
    # Copy DLL to package dir for runtime availability
    dll_src = os.path.join(libsodium_lib_dir, 'libsodium.dll')
    dll_dst = os.path.join('kadepy', 'libsodium.dll')
    if os.path.exists(dll_src):
        print(f"Copying {dll_src} to {dll_dst}")
        shutil.copy2(dll_src, dll_dst)

hyperswarm_module = Extension(
    'kadepy._hyperswarm',
    sources=hyperswarm_sources,
    include_dirs=hyperswarm_includes,
    define_macros=hyperswarm_macros,
    extra_compile_args=extra_compile_args,
    libraries=hyperswarm_libraries,
    library_dirs=hyperswarm_library_dirs,
)

# Read long description from README
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='kadepy',
    version='0.2.3',
    description='High-performance P2P Swarm implementation in C/Python with Noise/UDX',
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=['kadepy'],
    package_data={'kadepy': ['*.dll', '*.pyd']},
    include_package_data=True,
    ext_modules=[module, hyperswarm_module],
    python_requires='>=3.10',
    url="https://github.com/ON00dev/KadePy",
    author="KadePy Contributors",
    author_email="on00dev.dev@gmail.com",
    license="MIT",
    classifiers=[
        "Development Status :: 3 - Alpha",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Programming Language :: C",
        "Topic :: System :: Networking",
    ],
)
