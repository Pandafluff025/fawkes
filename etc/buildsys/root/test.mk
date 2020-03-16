#*****************************************************************************
#                      Makefile Build System for Fawkes
#                            -------------------
#   Created on Mon 16 Mar 2020 09:09:58 CET
#   Copyright (C) 2020 by Till Hofmann <hofmann@kbsg.rwth-aachen.de>
#
#*****************************************************************************
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
#
#*****************************************************************************

test: exec_testscripts
exec_testscripts:
	$(SILENT) if [ -d $(TOP_BASEDIR)/tests.d/ ] ; then \
		for test in $$(ls $(TOP_BASEDIR)/tests.d/) ; do \
			if [ -x $(TOP_BASEDIR)/tests.d/$$test ] ; then \
				echo -e $(INDENT_PRINT)"[TEST] Executing test $(TOP_BASEDIR)/tests.d/$$test"; \
				exec $(TOP_BASEDIR)/tests.d/$$test | sed s'/^/$(INDENT_PRINT)[TEST] /'; \
				if [ $${PIPESTATUS[0]}  -ne 0 ] ; then \
					echo -e $(INDENT_PRINT)"[TEST] $(TBOLDRED)Failed test: $(TOP_BASEDIR)/tests.d/$$test$(TNORMAL)"; \
					exit 1; \
				fi \
			fi \
		done \
	fi
