#!/bin/bash -ex

SITE=redev
BRANCH=gh-pages

if [ -d $SITE ]; then
   cd $SITE && git checkout main && git clean -df && git pull && cd ..
else
   git clone git@github.com:SCOREC/${SITE}.git
fi || {
   echo "Error cloning/updating '$SITE'."
   exit 1
}

pyEnv=redevDocsPyEnv
if [ ! -d $pyEnv ]; then
  python3 -m venv $pyEnv
  source $pyEnv/bin/activate
  python -m pip install sphinx-rtd-theme
  python -m pip install breathe
  python -m pip install sphinx-sitemap
  python -m pip install sphinx
else
  source $pyEnv/bin/activate
fi

doxyVer=$(doxygen --version)
echo "Doxygen version $doxyVer"
if [[ $doxyVer == 1.8.5 ]]; then
  ln -s Doxyfile.185 Doxyfile
elif [[ $doxyVer == 1.9.3 ]]; then
  ln -s Doxyfile.193 Doxyfile
else 
  echo "unsupported Doxygen version $doxyVer"
  exit 1
else

cd $SITE/docsDoxySphinx
make html
git checkout gh-pages
cp -r build/html/* ../docs
git add -A ../docs
git commit -m "Deploying the Redev documentation"
git push

