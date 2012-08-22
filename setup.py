#!/usr/bin/env python

from distutils.core import setup, Extension

module1 = Extension('PorterStemmer', sources = ['porter_stemmer.cpp'])

setup (name = 'PorterStemmer',
        version = '1.0',
        description = 'Faster implementation of PorterStemmer',
        ext_modules = [module1],
        author = 'Keith Bussell')
