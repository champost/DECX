#!/bin/bash

# Use this script to generate consistent singularity / docker definition files.

# Collect in this temporary variable.
DEPS=""
deps() {
    DEPS="${DEPS} $@"
}

# "Build" dependencies are removed after compilation.
BUILD_DEPS=""
bdeps() {
    BUILD_DEPS="${BUILD_DEPS} $@"
}

#==== Dependencies =============================================================
# Build-time only dependencies.
bdeps git             # To get source code.
bdeps cmake make      # Build tools.
bdeps gcc gcc-fortran # Compilers
bdeps boost           # Template lib.
bdeps pacman-contrib  # For paccache.

# Runtime dependencies.
deps openblas lapack gsl

#=== Construction layers =======================================================
read -r -d '' DEPENDENCIES_LAYER <<-EOF
    #---- Dependencies ---------------------------------------------------------
    echo 'Server = https://mirrors.kernel.org/archlinux/\$repo/os/\$arch' \\
         > /etc/pacman.d/mirrorlist
    pacman -Syu --noconfirm
    pacman -Sy --noconfirm ${BUILD_DEPS} ${DEPS}
    #---------------------------------------------------------------------------
EOF

read -r -d '' COMPILATION_LAYER <<-'EOF'
    #---- Compilation ----------------------------------------------------------
    # Get source code.
    cd /opt
    git clone --recursive https://github.com/champost/DECX
    cd DECX

    # Compile
    mkdir build
    cd build
    cmake -DCMAKE_BUILD_TYPE=Release ../src
    make -j $(nproc)
    #---------------------------------------------------------------------------
EOF

read -r -d '' INSTALLATION_LAYER <<-EOF
    #---- Installation ---------------------------------------------------------
    ln -s /opt/DECX/build/decx /usr/bin/decx
    #---------------------------------------------------------------------------
EOF

read -r -d '' CLEANUP_LAYER <<-EOF
    #---- Cleanup --------------------------------------------------------------
    # Remove the packages downloaded to image's Pacman cache dir.
    paccache -r -k0

    # Uninstall build dependencies.
    pacman -Rns --noconfirm ${BUILD_DEPS}
    #---------------------------------------------------------------------------
EOF

#=== Generate Singularity file =================================================

FILENAME="singularity.def"
cat <<EOF > $FILENAME
BootStrap: docker
From: archlinux

# This file was automatically generated from ./containerize.bash.

%post
    echo "Building container.."

    ${DEPENDENCIES_LAYER}

    ${COMPILATION_LAYER}

    ${INSTALLATION_LAYER}

    ${CLEANUP_LAYER}

    echo "export CONTAINER_BUILD_TIME=\\"\$(date)\\"" \\
        >> \${SINGULARITY_ENVIRONMENT}

%runscript
    echo "Running DECX container (created on \${CONTAINER_BUILD_TIME})"
    decx \$@
EOF
echo "Generated $FILENAME"

#=== Generate Docker file ======================================================

FILENAME="Dockerfile"
cat <<DEOF > $FILENAME
# syntax=docker/dockerfile:1.3-labs
FROM archlinux

# This file was automatically generated from ./containerize.bash.

RUN <<EOF
    ${DEPENDENCIES_LAYER}
EOF

RUN <<EOF
    ${COMPILATION_LAYER}
EOF

RUN <<EOF
    ${INSTALLATION_LAYER}
EOF

RUN <<EOF
    ${CLEANUP_LAYER}
EOF

# Now pick a folder to work within.
RUN mkdir -p /home/decx
WORKDIR /home/decx

ENTRYPOINT ["decx"]
DEOF
echo "Generated $FILENAME"

exit 0
