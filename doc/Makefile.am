SUBDIRS = guide

EXTRA_DIST = version.ent.in

devel:
	mkdir -p html
	(cat Doxyfile; echo PROJECT_NUMBER=$(PACKAGE_VERSION)) | doxygen - > html/doxygen.log 2> html/doxygen-error.log
	echo `grep -i ": warning:" html/doxygen-error.log | wc -l` warnings/errors detected.

clean:
	rm -rf html

# That's a bit of a hack, but we are making sure that devel target
# is always valid. The alternative is to make devel depend on all
# *.cc *.h files in the whole tree.
.PHONY: devel
