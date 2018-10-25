#for build libraries
CC               := gcc
INC_PATH         := ./src
SRC_PATH         := ./src
SAMPLE_SRC_PATH  := ./sample
LIB_PATH_RELEASE := ./lib/release
LIB_PATH_DEBUG   := ./lib/debug
OBJ_PATH_RELEASE := ./obj/release
OBJ_PATH_DEBUG   := ./obj/debug
CFLAGS_RELEASE   := -Wall -O3 
CFLAGS_DEBUG     := -Wall -g3 -O0 
TARGET_RELEASE   := libprompt.a liboption.a
TARGET_DEBUG     := libprompt_debug.a liboption_debug.a

#for build sample app
CFLAGS_SAMPLE :=  -Wall -lprompt_debug -loption_debug -lreadline

#for install
LIB_DIR  := /usr/lib
INC_DIR  := /usr/include/consoleapp

vpath %.h $(INC_PATH)
vpath %.c $(SRC_PATH) $(SAMPLE_SRC_PATH)
vpath %.o $(OBJ_PATH_RELEASE) $(OBJ_PATH_DEBUG)
vpath %.a $(LIB_PATH_RELEASE) $(LIB_PATH_DEBUG) 

.PHONY: clean tag install uninstall 

all: 
	@for r in $(TARGET_RELEASE); \
	do make $$r;                 \
	done
	@for d in $(TARGET_DEBUG);   \
	do make $$d;                 \
	done
	make sample
	ctags -R

sample: sample.c $(TARGET_DEBUG)
	$(CC) $(CFLAGS_DEBUG) -I$(INC_PATH) -L$(LIB_PATH_DEBUG) -o$(SAMPLE_SRC_PATH)/$@ $< $(CFLAGS_SAMPLE)

lib%_debug.a: %_debug.o
	mkdir -p $(LIB_PATH_DEBUG)
	ar rcs $@ $(OBJ_PATH_DEBUG)/$^
	mv $@ $(LIB_PATH_DEBUG)

lib%.a: %.o
	mkdir -p $(LIB_PATH_RELEASE)
	ar rcs $@ $(OBJ_PATH_RELEASE)/$^
	mv $@ $(LIB_PATH_RELEASE)

%_debug.o: %.c %.h 
	mkdir -p $(OBJ_PATH_DEBUG)
	$(CC) $(CFLAGS_DEBUG) -I$(INC_PATH) -o$@ -c $(SRC_PATH)/$*.c
	mv $@ $(OBJ_PATH_DEBUG)

%.o: %.h %.c confing.h
	mkdir -p $(OBJ_PATH_RELEASE)
	$(CC) $(CFLAGS_RELEASE) -I$(INC_PATH) -c $(SRC_PATH)/$*.c
	mv $@ $(OBJ_PATH_RELEASE)

confing.h:
	touch src/config.h

install:
	sudo mkdir $(INC_DIR)
	@for r in $(TARGET_RELEASE);                     \
	do                                              \
		make $$r;                                   \
		sudo cp $(LIB_PATH_RELEASE)/$$r $(LIB_DIR); \
		sudo cp $(INC_PATH)/*.h $(INC_DIR);         \
	done

uninstall:
	sudo rm -rf $(INC_DIR)
	@for r in $(TARGET_RELEASE);    \
	do                             \
		sudo rm -f $(LIB_DIR)/$$r; \
	done

tag:
	@ctags -R --language-force=C

clean:
	find . -name "*.[ao]" | xargs rm -f
	rm -rf obj
	rm -rf lib
	rm -f tags
	rm -f $(SAMPLE_SRC_PATH)/sample
