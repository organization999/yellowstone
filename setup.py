"""Setuptools entry point for Yellowstone."""
from setuptools import setup
from buildsystem.package import configuration

setup(**configuration())
