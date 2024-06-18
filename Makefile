TARGET := iphone:clang:latest:7.0
ARCHS = arm64

include $(THEOS)/makefiles/common.mk

TOOL_NAME = oldabichecker

oldabichecker_FILES = main.c
oldabichecker_CODESIGN_FLAGS = -Sentitlements.plist
oldabichecker_INSTALL_PATH = /usr/local/bin

include $(THEOS_MAKE_PATH)/tool.mk