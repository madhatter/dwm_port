--- ./config.mk.orig	2011-12-19 16:02:46.000000000 +0100
+++ ./config.mk	2014-03-24 14:38:46.216409999 +0100
@@ -4,30 +4,34 @@
 # Customize below to fit your system
 
 # paths
-PREFIX = /usr/local
-MANPREFIX = ${PREFIX}/share/man
+PREFIX = /usr/local 
+MANPREFIX = /usr/local
 
-X11INC = /usr/X11R6/include
-X11LIB = /usr/X11R6/lib
+X11INC = /usr/local/include
+X11LIB = /usr/local/lib
 
 # Xinerama
 XINERAMALIBS = -L${X11LIB} -lXinerama
 XINERAMAFLAGS = -DXINERAMA
 
+# Xft
+XFTINCS = -I${X11INC}/freetype2
+XFTLIBS = -L${X11LIB} -lXft
+
 # includes and libs
-INCS = -I. -I/usr/include -I${X11INC}
-LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 ${XINERAMALIBS}
+INCS = -I. -I/usr/include -I${X11INC} 
+LIBS = -L/usr/lib -lc -L${X11LIB} -lX11 ${XINERAMALIBS} 
 
 # flags
-CPPFLAGS = -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
+CPPFLAGS+= -DVERSION=\"${VERSION}\" ${XINERAMAFLAGS}
 #CFLAGS = -g -std=c99 -pedantic -Wall -O0 ${INCS} ${CPPFLAGS}
-CFLAGS = -std=c99 -pedantic -Wall -Os ${INCS} ${CPPFLAGS}
+CFLAGS+= -std=c99 ${INCS} ${CPPFLAGS}
 #LDFLAGS = -g ${LIBS}
-LDFLAGS = -s ${LIBS}
+LDFLAGS+= ${LIBS}
 
 # Solaris
 #CFLAGS = -fast ${INCS} -DVERSION=\"${VERSION}\"
 #LDFLAGS = ${LIBS}
 
 # compiler and linker
-CC = cc
+CC?= cc
