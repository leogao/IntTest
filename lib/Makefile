###############################################################################
# @file  Makefile
#
# GPL LICENSE SUMMARY
# 
#   Copyright(c) 2010 Intel Corporation. All rights reserved.
# 
#   This program is free software; you can redistribute it and/or modify 
#   it under the terms of version 2 of the GNU General Public License as
#   published by the Free Software Foundation.
# 
#   This program is distributed in the hope that it will be useful, but 
#   WITHOUT ANY WARRANTY; without even the implied warranty of 
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
#   General Public License for more details.
# 
#   You should have received a copy of the GNU General Public License 
#   along with this program; if not, write to the Free Software 
#   Foundation, Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
#   The full GNU General Public License is included in this distribution 
#   in the file called LICENSE.GPL.
# 
#   Contact Information:
#   Intel Corporation
# 
#  version: FPGA_CPRI.L.0.1.4-14
###############################################################################

EXTRA_CFLAGS+=-D_GNU_SOURCE
CFLAGS+=-fPIC $(DEBUGFLAGS) -Wall -Wpointer-arith $(INCLUDES)

ifdef DEBUG
EXTRA_CFLAGS+=-DDEBUG 
endif

EXTRA_CFLAGS+=-I$(PWD)/../include

latint_api.o: latint_api.c
	${CC} -o $@ -c $< ${CFLGAS} ${EXTRA_CFLAGS}

latint.a: latint_api.o
	@echo ${AR} r $@ $<; \
	${AR} r $@ $<;


all: latint.a
clean:
	rm -f *.o
	rm -f latint.a
