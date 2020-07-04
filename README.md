# ttygraphics
Playing with 3D Graphics, Floating-Point Numbers, and drawing straight the the Framebuffer(Or other kernel interfaces)  
  
--------------------------------------------------------------------------------  

# WARNING!! (DO NOT SKIP):  

In some versions of this program, during launch, low-level kernel calls to change  
the underlying settings of the kernel TTY are made.  These settings should be reverted upon exit.  
The program forks itself to run most processing as a child of itself.  This is done  
so that in the event of a crash, the program can attempt to recover the TTY before exiting.  
However, I offer no guarentees.  Should the TTY become borked, it may become difficult if not  
impossible to recover to interface to the system without a hard reset.  During development, I  
had to SSH into my system, write, and run as root a new program, in the terminal, that would make  
the necessary calls to the kernel, to recover the TTY when this happened.  The TTY failed in such  
a way as to no long even accept Ctrl+Alt+F[x] commands to change TTYs to one that worked.  I am  
including a supplementary program source file that you can build and run to try to do this for you  
should you encounter the same problem.  To run it, would will probably have to have an alternative interface  
available, like SSH.  The file is called: "fix_my_tty.c".  GCC should compile it without any addtional  
options.  
  
--------------------------------------------------------------------------------  
  
Compatibility:  
--This has only been tested on x86-64 Linux.  
--This program writes directly to the Linux Kernel Frame Buffer.  
--It must be run from a raw kernel tty and as a user with the necessary privileges to write to the Frame Buffer  
  
Building:  
1) Run from inside the root directory: "gcc -std=gnu99 -lm -lpthread ./3d_graphics.c -o ./3d_graphics.out"  
  
Running:  
1) Switch to a raw kernel TTY.  This can usually be done with Ctrl+Alt+F[1-7].  If you are in a  
   virtual machine, there is usually a way to pass that command to the guest system.  Refer to your  
   hypervisor's documentation for more information.  
2) Login as a user with the necessary privileges to write to the Framebuffer.  On some Linux  
   distributions, this can be as easy as adding your user to some user group, such as "video".  
3) Run from inside the root directory: "./3d_graphics.out"  
  
TODO:  
--Stop camera from drifting due to floating point rounding errors  
--Fix Line Drawing so it works correctly if one or more points are out of FOV  
--Figure out how to do and implement VSYNC to avoid screen tearing  
--Look into more sophisticated and modern kernel video display interfaces  
----Look into GPU acceleration  
--Create libc alteratives to support freestanding compilation  
--Comment and clean-up code  
