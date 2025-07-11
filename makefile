#makefile
#Name: muhammad ali
#If you want to see all the debug print statements, uncomment -DDEBUG on line 6. 

CC		= gcc
CFLAGS		= -pedantic -Wall  #-DDEBUG

EXE		= tokensim

TARFILE		= A2.tar

OBJS		= \
		tokenRing_main.o \
		tokenRing_setup.o \
		tokenRing_simulate.o

$(EXE) : $(OBJS)
	$(CC) -o $(EXE) $(OBJS)

clean :
	@ rm -f $(OBJS)

$(TARFILE) tarfile tar :
	tar cvf $(TARFILE) README *.c *.h makefile

tokenRing_main.o : tokenRing_main.c tokenRing.h
tokenRing_setup.o : tokenRing_setup.c tokenRing.h
tokenRing_simulate.o : tokenRing_simulate.c tokenRing.h
