#!/usr/bin/env bash

set -x
pandoc \
	-f markdown+footnotes \
	--number-sections    \
	--table-of-contents  \
	--output README.pdf  \
	--pdf-engine=xelatex \
	--variable geometry=margin=1.5cm \
	README.md
