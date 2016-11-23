#!/usr/bin/env python3

import argparse
import difflib
import logging
import os
import re
import sys
from distutils.version import LooseVersion

import common

# The version we care about
MIN_VERSION_REQUIRED = LooseVersion('3.8.0')

# Name of clang-format as a binary
PROGNAME = 'clang-format'


class ClangFormatter:
    def __init__(self, path_to_cf: str) -> None:
        self.exepath = path_to_cf

    def __str__(self):
        return self.exepath

    @property
    def version(self) -> LooseVersion:
        return LooseVersion(
            common.callo(' '.join([self.exepath, '--version'])).split()[2]
        )

    # Format a given source_file
    def format(self, source_file: str) -> str:
        cmd = ' '.join([self.exepath, '--style=file', source_file])
        return common.callo(cmd)

    # Lint a given file - return whether or not the file is formatted correctly
    def lint(self, source_file: str, show_diff: bool=True) -> bool:
        with open(source_file, 'r', errors='ignore') as original_file:
            original_text = original_file.read()

        original_lines = original_text.splitlines()
        formatted_lines = self.format(source_file).splitlines()

        # Create some nice git-like metadata for the diff
        base_git_dir = common.callo('git rev-parse --show-toplevel')
        relpath = os.path.relpath(source_file, base_git_dir)
        fromfile = os.path.join('a', relpath)
        tofile = os.path.join('b', relpath)

        diff = list(
            difflib.unified_diff(
                original_lines,
                formatted_lines,
                fromfile=fromfile,
                tofile=tofile
            )
        )

        if diff:
            print('Found formatting changes for file:', source_file)

            if show_diff:
                print(
                    'To fix, run "{} --style=file -i {}"'.
                    format(cf, source_file)
                )
                print('Suggested changes:')
                for line in diff:
                    print(line.strip())
                print()

        return not diff


if __name__ == '__main__':
    # Set up some logging
    logging.basicConfig(
        stream=sys.stdout,
        level=logging.DEBUG,
        format='%(asctime)s %(name)s %(levelname)s %(message)s',
        datefmt='%m-%d %H:%M',
    )

    parser = argparse.ArgumentParser('clang-format')
    parser.add_argument(
        '-c',
        '--clang-format-path',
        help='path to clang-format',
        metavar='PATH',
        dest='path',
    )
    parser.add_argument(
        '--no-diff',
        help='hide file diff generated by clang-format',
        default=False,
        action='store_true',
    )
    args = parser.parse_args()
    show_diff = not args.no_diff

    # Find clang-format, validate version
    path_to_cf = common.find_binary(args.path, PROGNAME)
    cf = ClangFormatter(path_to_cf)
    if cf.version < MIN_VERSION_REQUIRED:
        logging.error(
            '''Incorrect version of {}: got {} but at least {} is required'''
            .format(PROGNAME, cf.version, MIN_VERSION_REQUIRED)
        )
        sys.exit(1)

    # Find changed files from last common ancestor with develop
    changes = common.changed_files(filter_regex=r'\.(h|hpp|c|cpp)$')
    if not changes:
        logging.info('No changed files, exiting')
        sys.exit(0)

    # Validate each with clang-format
    clean = all([cf.lint(f, show_diff=show_diff) for f in changes])

    sys.exit(0) if clean else sys.exit(1)
