## Installation requirements
 - OpenCL headers and ICD drivers
 - xxd - a hex dump utility needed by automake to inline OpenCL & OpenGL code
 - automake utilities
 - gcc/g++

## Hands on experience
It's been tested on Fedora, Debian and MacOS using the distros' required packages

On Fedora 39 with `$XDG_SESSION_TYPE=x11`, using  nvidia-driver of version 535.154.05 on nVIDIA RTX 3090, it generates 
approx. 530fps of heat map of 2D lagrange interpolation over 8x8 points net and with resolution of 1600x1600 pixels.
