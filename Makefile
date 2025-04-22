# Group Members: Vincent Roberson and Muhammad I Sohail
# ECEE 446 Section 1
# Spring 2025
EXE = program4
CFLAGS = -Wall
CXXFLAGS = -Wall
LDLIBS =
CC = gcc
CXX = g++

.PHONY: all
all: $(EXE)

# Implicit rules defined by Make, but you can redefine if needed
#
#program4: program4.c
#	$(CC) $(CFLAGS) program4.c $(LDLIBS) -o program4
#
# OR
#
#program4: program4.cc
#	$(CXX) $(CXXFLAGS) program4.cc $(LDLIBS) -o program4

.PHONY: clean
clean:
	rm -f $(EXE)