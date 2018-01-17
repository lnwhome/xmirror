#!/bin/sh

x11vnc -display :0.1 -scale 0.5 -rotate 0 \
	-localhost -nopw -norc -timeout 3 \
	-q -viewonly -once -bg && \
vncviewer localhost
