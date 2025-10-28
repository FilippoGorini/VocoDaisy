# Project Name
TARGET = VocoDaisy

# Source files
CPP_SOURCES = $(wildcard src/*.cpp)

# Add include folder for headers
C_INCLUDES += -Iinclude

# Use C++17
CPP_STANDARD = -std=c++17

# Library Locations
LIBDAISY_DIR = ../DaisyExamples/libDaisy/
DAISYSP_DIR  = ../DaisyExamples/DaisySP/

# Core location, and generic Makefile
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile







# # Project Name
# TARGET = VocoDaisy

# # Sources
# CPP_SOURCES = VocoDaisy.cpp

# # Library Locations
# LIBDAISY_DIR = ../DaisyExamples/libDaisy/
# DAISYSP_DIR = ../DaisyExamples/DaisySP/

# # Core location, and generic Makefile.
# SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
# include $(SYSTEM_FILES_DIR)/Makefile
