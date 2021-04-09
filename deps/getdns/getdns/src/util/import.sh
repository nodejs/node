#!/bin/sh

REPO=http://unbound.net/svn/trunk

wget -O rbtree.c			${REPO}/util/rbtree.c
wget -O orig-headers/rbtree.h		${REPO}/util/rbtree.h
wget -O val_secalgo.c			${REPO}/validator/val_secalgo.c
wget -O orig-headers/val_secalgo.h	${REPO}/validator/val_secalgo.h
wget -O lruhash.c			${REPO}/util/storage/lruhash.c
wget -O orig-headers/lruhash.h		${REPO}/util/storage/lruhash.h
wget -O lookup3.c			${REPO}/util/storage/lookup3.c
wget -O orig-headers/lookup3.h		${REPO}/util/storage/lookup3.h
wget -O locks.c				${REPO}/util/locks.c
wget -O orig-headers/locks.h		${REPO}/util/locks.h
