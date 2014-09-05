#!/usr/bin/python

try:
    from setuptools import setup
except ImportError:
    from distutils.core import setup

config = {
    'name': 'pcp2pdf',
    'version': '0.1',
    'author': 'Michele Baldessari',
    'author_email': 'michele@acksyn.org',
    'url': 'http://pcp.io',
    'license': 'GPLv2',
    'py_modules': ['pcp2pdf_archive', 'pcp2pdf_stats', 'pcp2pdf_style'],
    'scripts': ['pcp2pdf'],
    'classifiers': [
        "Development Status :: 3 - Alpha",
        "Topic :: Utilities",
        "License :: OSI Approved :: GNU General Public License v2 (GPLv2)",
        "Programming Language :: Python",
    ],
}

setup(**config)
