from setuptools import setup, find_packages
from Cython.Build import cythonize
import numpy as np
setup(
    name='EQdetect',
    version='0.1',
    packages=find_packages(),
    ext_modules=cythonize(["EQdetect/**/*.pyx"]),
    install_requires=[
        'mysql-connector',
    ],
    include_dirs=[np.get_include()]
)