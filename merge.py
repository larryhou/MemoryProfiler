#!/usr/bin/env python

import lxml.etree as XML
import os.path as p
import subprocess, os, shutil

def system(command):
    print('+ {}'.format(command))
    assert os.system(command) == 0

def merge_revision(src_rep, dst_rep, revision):
    project = p.basename(src_rep)
    if not p.exists(project):
        command = 'svn co --username next_ci --password \'#N1e8X6t$\' --no-auth-cache --depth empty {}'.format(dst_rep)
    else:
        command = 'svn up --username next_ci --password \'#N1e8X6t$\' --no-auth-cache --depth empty {}'.format(project)
    system(command)

    curcwd = os.getcwd()
    os.chdir(project)

    revision_number = int(revision)
    command = 'svn log --username next_ci --password \'#N1e8X6t$\' --no-auth-cache -r {} {} --verbose --xml'.format(revision_number, src_rep)
    process = subprocess.Popen(command, shell=True, stderr=subprocess.PIPE, stdout=subprocess.PIPE)
    stdout, stderr = process.communicate()
    assert process.returncode == 0, stderr
    data = XML.fromstring(stdout)
    for node in data.xpath('//paths/path'):
        kind = node.get('kind') # type: str
        text = node.text # type: str
        relpath = text.split(project + '/')[1]
        assert relpath
        if kind == 'file':
            command = 'svn update --username next_ci --password \'#N1e8X6t$\' --no-auth-cache --parents {}'.format(relpath)
        else:
            command = 'svn update --username next_ci --password \'#N1e8X6t$\' --no-auth-cache --parents --depth empty {}'.format(relpath)
        system(command)
    command = 'svn merge --username next_ci --password \'#N1e8X6t$\' --no-auth-cache -r {}:{} {}'.format(revision_number-1, revision_number, src_rep)
    system(command)
    os.chdir(curcwd)

def main():
    import argparse, sys
    arguments = argparse.ArgumentParser()
    arguments.add_argument('--revision', '-r', nargs='+', required=True)
    arguments.add_argument('--rep-to', '-t', default='god_trunk_PublishReview_150813_20190423')
    arguments.add_argument('--rep-from', '-f', default='god_trunk')
    arguments.add_argument('--project', '-p', default='TheNextMOBA', choices=('Logic', 'MorefunLockStep', 'TheNextMOBA', 'SimplePlaymaker', 'PlayMaker'))
    options = arguments.parse_args(sys.argv[1:])
    rep_base_url = 'http://tc-svn.tencent.com/ied/ied_narutoNext_rep/naruto_next_proj/release'
    dst_rep = '{}/{}/{}'.format(rep_base_url, options.rep_to, options.project)
    src_rep = '{}/{}/{}'.format(rep_base_url, options.rep_from, options.project)
    if p.exists(options.project): shutil.rmtree(options.project)
    for revision in options.revision:
        merge_revision(src_rep=src_rep, dst_rep=dst_rep, revision=revision)

if __name__ == '__main__':
    main()