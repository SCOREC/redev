## Documentation

The documentation is built using Doxygen and formatted with Sphinx with the
ReadTheDocs theme.  The breathe extension is used to access the xml output
containing class, struct, function, etc. documentation from Doxygen in
restructured text (.rst) files.

The `.nojekyll` file in the `docs` directory of the `gh-pages` branch is required for the content
to render correctly on Github pages. See
https://docs.github.com/en/pages/getting-started-with-github-pages/about-github-pages#static-site-generators
for more info.

### Building

See `setupPythonEnv.sh` for the list of required Python packages.

Once the environment is setup run the following commands to create the docs.  The contents
of the `build/html` directory needs to be uploaded to the `docs` directory in the
`gh-pages` branch for it to be accessible at https://scorec.github.io/redev/.

```
git checkout main
cd docsDoxySphinx
make html
```

#### Publishing

There are Github actions that can automate publication but they have many
dependencies and I have not found one that is created by Github or a 'verified'
creator.  So, in the interest of security and keeping things simple, the
following commands need to be run locally to publish new documentation.  They
assume `make html` was already ran.

```
git checkout gh-pages
cp -r docsDoxySphinx/build/html docs
git add -A docs
git commit -m "docs: update" #or something like this
git push
```

