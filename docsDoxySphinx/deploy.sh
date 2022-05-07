#!/bin/sh
set -e

SITE=redev
BRANCH=gh-pages

if [ -d $SITE ]; then
   cd $SITE && git checkout . && git clean -df && git pull && cd ..
else
   git clone -b ${BRANCH} git@github.com:SCOREC/${SITE}.git
fi || {
   echo "Error cloning/updating '$SITE'."
   exit 1
}

#pull from main
git fetch
git merge --no-ff origin/main
cd $SITE/docs
make html
git add -A .
git commit -m "Deploying the Redev documentation"
git push

