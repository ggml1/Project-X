# server code directory name
SERVERDIR := codigos/server

# common code directory name
COMMONDIR := codigos/common

# binary output directory name
OUTPUTDIR := bin

# name for the output(binary) of the server code
SERVERNAME := server

# libary code directory name
LIBDIR := lib

# add C flags, like: CFLAGS := -Wall -Werror -Wconversion -Wextra
CFLAGS :=

# add load flags, others like -pthread
LDLIB := -lm -lallegro -lallegro_image -lallegro_primitives -lallegro_font -lallegro_ttf

CC := gcc -std=c99
RM := rm -f
MK := mkdir -p

EXT := c
INC := -I $(LIBDIR) -I $(COMMONDIR)

SERVERSOURCES := $(shell find $(SERVERDIR) -type f -name *.$(EXT))
LIBSOURCES := $(shell find $(LIBDIR) -type f -name *.$(EXT))
COMMONSOURCES := $(shell find $(COMMONDIR) -type f -name *.$(EXT))

SERVEROBJS := $(subst .$(EXT),.o,$(SERVERSOURCES))
LIBOBJS := $(subst .$(EXT),.o,$(LIBSOURCES))
COMMONOBJS := $(subst .$(EXT),.o,$(COMMONSOURCES))

all: mkdirs buildServer clean

server: mkdirs buildServer clean runServer

buildServer: $(LIBOBJS) $(SERVEROBJS) $(COMMONOBJS)
	@echo "\n  Linking $(SERVERNAME)..."
	$(CC) -o $(OUTPUTDIR)/$(SERVERNAME) $(LIBOBJS) $(COMMONOBJS) $(SERVEROBJS) $(LDLIB) $(CFLAGS)
	@echo "\n"

%.o : %.$(EXT)	
	$(CC) -c $< -o $@ $(INC) $(CFLAGS)

mkdirs:
	$(MK) $(OUTPUTDIR)

clean:
	@echo "  Cleaning..."
	$(RM) $(LIBOBJS) $(SERVEROBJS) $(COMMONOBJS)

runServer:
	@echo "\n  Starting to run $(SERVERNAME)...\n"; sudo ./$(OUTPUTDIR)/$(SERVERNAME) 
