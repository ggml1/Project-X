# client code directory name
CLIENTDIR := codigos/client

# common code directory name
COMMONDIR := codigos/common

# binary output directory name
OUTPUTDIR := codigos/client

# name for the output(binary) of the client code
CLIENTNAME := client

# libary code directory name
LIBDIR := lib

# add C flags, like: CFLAGS := -Wall -Werror -Wconversion -Wextra
CFLAGS := `pkg-config --cflags --libs opencv` -fopenmp

# add load flags, others like -pthread
LDLIB := -lm -lallegro -lallegro_image -lallegro_primitives -lallegro_font -lallegro_ttf

CC := g++ -std=c++0x
RM := rm -f
MK := mkdir -p

EXT := cpp
INC := -I $(LIBDIR) -I $(COMMONDIR)

CLIENTSOURCES := $(shell find $(CLIENTDIR) -type f -name *.$(EXT))
LIBSOURCES := $(shell find $(LIBDIR) -type f -name *.$(EXT))
COMMONSOURCES := $(shell find $(COMMONDIR) -type f -name *.$(EXT))

CLIENTOBJS := $(subst .$(EXT),.o,$(CLIENTSOURCES))
LIBOBJS := $(subst .$(EXT),.o,$(LIBSOURCES))
COMMONOBJS := $(subst .$(EXT),.o,$(COMMONSOURCES))

all: mkdirs buildClient clean

client: mkdirs buildClient clean runClient

buildClient: $(LIBOBJS) $(CLIENTOBJS) $(COMMONOBJS)
	@echo "\n  Linking $(CLIENTNAME)..."
	$(CC) -o $(OUTPUTDIR)/$(CLIENTNAME) $(LIBOBJS) $(COMMONOBJS) $(CLIENTOBJS) $(LDLIB) $(CFLAGS)
	@echo "\n"

%.o : %.$(EXT)	
	$(CC) -c $< -o $@ $(INC) $(CFLAGS)

mkdirs:
	$(MK) $(OUTPUTDIR)

clean:
	@echo "  Cleaning..."
	$(RM) $(LIBOBJS) $(CLIENTOBJS) $(COMMONOBJS)

runClient:
	@echo "\n  Starting to run $(CLIENTNAME)...\n"; ./$(OUTPUTDIR)/$(CLIENTNAME)