#! /bin/sh

git add '*.orig'
git add '*.c'
git add '*.cpp'
git add '*.h'
git add '*.asm'
git add '*.s'
git add '*.S'
git add '*.ld'
git add '*.nb'

find . -name '.cproject' | xargs git add
find . -name '.project' | xargs git add

find . -name 'README' | xargs git add
git add '*.readme'
git add '*.md'
git add '*.txt'
git add '*.url'
git add '*.sh'
