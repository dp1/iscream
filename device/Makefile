# name of your application
APPLICATION = iscream

# If no BOARD is found in the environment, use this default:
# BOARD ?= nucleo-f401re
BOARD ?= b-l475e-iot01a

# This has to be the absolute path to the RIOT base directory:
RIOTBASE ?= $(CURDIR)/../../RIOT

# Comment this out to disable code in RIOT that does safety checking
# which is not needed in a production environment but helps in the
# development process:
DEVELHELP ?= 1

# Change this to 0 show compiler invocation lines by default:
QUIET ?= 1

USEMODULE += xtimer
USEMODULE += isrpipe
USEMODULE += periph_gpio_irq

include $(RIOTBASE)/Makefile.include
