from setuptools import setup, Extension
import sys
import os

# Definição das macros para Windows/MSVC
if sys.platform == 'win32':
    extra_compile_args = ['/W4', '/O2'] # Warning level 4, Optimization level 2
    define_macros = [('WIN32', '1')]
    libraries = ['ws2_32', 'bcrypt']
else:
    extra_compile_args = ['-Wall', '-O3']
    define_macros = []
    libraries = []

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

# Configuração da Extensão
module = Extension(
    'kadepy._core',
    sources=sources,
    include_dirs=['src'],
    define_macros=define_macros,
    extra_compile_args=extra_compile_args,
    libraries=libraries,
)

# Read long description from README
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='kadepy',
    version='0.2.0',
    description='High-performance P2P Swarm implementation in C/Python',
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=['kadepy'],
    ext_modules=[module],
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
