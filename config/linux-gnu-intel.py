#!/usr/bin/env python

if __name__ == '__main__':
    import configure

    configure_options = [
        '--with-cc=icc',
	'--with-fc=ifc',
	'--with-cxx=icc',
        '--with-mpi-include=/home/petsc/soft/linux-rh73-intel/mpich-1.2.5.2/include',
        '--with-mpi-lib=[/home/petsc/soft/linux-rh73-intel/mpich-1.2.5.2/lib/libmpich.a,libpmpich.a]',
        '--with-mpirun=mpirun',
	'--with-blas-lapack-dir=/home/petsc/soft/linux-rh73-intel/mkl-52'
        ]

    configure.petsc_configure(configure_options)
