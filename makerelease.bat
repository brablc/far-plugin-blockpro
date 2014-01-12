@echo off
make
perl makeexamples.pl
make -f makerelease.mak
