Mandel V 1.8
==============

3 Sept 2010
by Jan Vlietinck, 
http://users.skynet.be/fquake/

Info
====

Very fast Mandelbrot and Julia renderer.
Requires a DX11 GPU.
The program detects if the GPU supports doubles
If not calculations will be done only with floats.



Controls
========

Space bar           : Toggles between Mandelbrot and Julia
M key               : Toggles between 1024 and 2048 maximum iterations

V key               : Vector calculations (only used for floats, not much effect for doubles)
S key               : Scalar calculations
F key               : Float  calculations
D key               : Double calculations
E key               : Toggle between two different double versions 
                      The first version is a straight conversion of the float version
                      However ATI currently is buggy and slow for this version.
                      The second version does less loop unrolling and works ok on ATI.
                      The first version is about 1/3 faster (at least currently on Nvidia)

A/Z keys            : Cycle colors
Mouse move          : Pan
Mouse move + SHIFT  : In Julia mode, move base point around to get a morphing fractal
Left and right
mouse buttons       : Zoom in/out
V key               : Synchronize frame rate to display refresh

