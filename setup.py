from setuptools import setup, find_packages

# Read long description from README
with open("README.md", "r", encoding="utf-8") as fh:
    long_description = fh.read()

setup(
    name='kadepy',
    version='0.3.0',
    description='High-performance P2P Swarm implementation via Official Hyperswarm Bindings',
    long_description=long_description,
    long_description_content_type="text/markdown",
    packages=find_packages(),
    include_package_data=True,
    python_requires='>=3.10',
    url="https://github.com/ON00dev/KadePy",
    author="KadePy Contributors",
    author_email="on00dev.dev@gmail.com",
    license="MIT",
    classifiers=[
        "Development Status :: 4 - Beta",
        "Intended Audience :: Developers",
        "License :: OSI Approved :: MIT License",
        "Programming Language :: Python :: 3",
        "Topic :: System :: Networking",
    ],
    install_requires=[
        # Add runtime dependencies here if any (e.g., 'requests', but we mostly use stdlib + nodejs)
    ],
)
