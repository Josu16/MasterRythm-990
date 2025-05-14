CIRCLEHOME = ../../circle

OBJS    = main.o kernel.o HyperNaturalSoundGenerator.o

LIBS	+=  $(CIRCLEHOME)/addon/fatfs/libfatfs.a \
		$(CIRCLEHOME)/lib/sched/libsched.a \
	   $(CIRCLEHOME)/lib/sound/libsound.a \
	   $(CIRCLEHOME)/lib/usb/libusb.a \
	   $(CIRCLEHOME)/lib/input/libinput.a \
	   $(CIRCLEHOME)/lib/fs/libfs.a \
	   $(CIRCLEHOME)/lib/libcircle.a

include $(CIRCLEHOME)/Rules.mk

-include $(DEPS)