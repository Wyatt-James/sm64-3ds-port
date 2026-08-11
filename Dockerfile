FROM devkitpro/devkitarm:20240202 AS build

RUN mkdir /docker_logs

# ----- Download library archives -----

WORKDIR /tmp

# ----- Install Misc. Packages -----
# I'm not sure why this is so broken. What the hell?
RUN rm /var/lib/apt/lists/* -rvf
RUN mv /etc/security/namespace.init /docker_logs/namespace.init
RUN apt-get clean
RUN apt-get update
RUN apt-get install -y binutils-mips-linux-gnu \
  bsdmainutils \
  build-essential \
  libaudiofile-dev \
  pkg-config \
  python3 \
  wget \
  unzip \
  zlib1g-dev

# CREATES makerom.zip
RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip \
  -O makerom.zip && \
  echo 976c17a78617e157083a8e342836d35c47a45940f9d0209ee8fd210a81ba7bc0  makerom.zip | sha256sum --check

# CREATES citro3d-wyatt-james.zip
RUN wget https://github.com/Wyatt-James/citro3d/archive/996898a63c2c26977172371bfb59e633b5144a9a.zip \
  -O citro3d-wyatt-james.zip && \
  echo 2C4940E73D109699A50BB54B84C60BA78952AE86874F64AB39311C8C827EB3B3  citro3d-wyatt-james.zip | sha256sum --check

# CREATES libctru-wyatt-james.zip
RUN wget https://github.com/Wyatt-James/libctru/archive/1f2ee50525131700f2f6744d26239ec611786228.zip \
  -O libctru-wyatt-james.zip && \
  echo BD09917C7EDC96A754EC6FE0EB35C01113F72904CFD1EFF7E4709138C4E42347  libctru-wyatt-james.zip | sha256sum --check

# ----- Extract archives in-place, removing commit-specific container folders -----
  
# CREATES citro3d-wyatt-james-temp, citro3d-wyatt-james
RUN unzip -d ./citro3d-wyatt-james-temp citro3d-wyatt-james.zip
RUN mv ./citro3d-wyatt-james-temp/citro3d-* ./citro3d-wyatt-james
  
# CREATES libctru-wyatt-james-temp, libctru-wyatt-james
RUN unzip -d ./libctru-wyatt-james-temp libctru-wyatt-james.zip
RUN mv ./libctru-wyatt-james-temp/libctru-* ./libctru-wyatt-james

# ----- Install dependencies -----

# Install wyatt-james's fork of libctru. Use the longer line to build with GDB-optimized debug data included.
# Removing this will leave the official devkitPro version installed.
WORKDIR /tmp/libctru-wyatt-james/libctru
RUN make install GPUCMD_DISABLE_BOUNDS_CHECKS=1 GPUCMD_INLINE_THRESH=0 GPUCMD_ENABLE_ZERO_PADDING=0 ENABLE_LTO=1 > /docker_logs/make_libctru-wyatt-james.txt
# RUN make install ARCH="-ggdb3 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft" > /docker_logs/make_libctru-wyatt-james.txt

# Install wyatt-james's fork of Citro3D. Use the longer line to build with GDB-optimized debug data included.
# Removing this will leave the official devkitPro version installed.
# The profiler defaults to off.
WORKDIR /tmp/citro3d-wyatt-james
RUN make install GPUCMD_DISABLE_BOUNDS_CHECKS=1 GPUCMD_INLINE_THRESH=0 GPUCMD_ENABLE_ZERO_PADDING=0 ENABLE_PROFILER=0 ENABLE_LTO=1 > /docker_logs/make_citro3d-wyatt-james.txt
# RUN make install ENABLE_PROFILER=0 > /docker_logs/make_citro3d-wyatt-james.txt
# RUN make install ARCH="-ggdb3 -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft" > /docker_logs/make_citro3d-wyatt-james.txt

# Install makerom
WORKDIR /tmp
RUN unzip -d /opt/devkitpro/tools/bin/ makerom.zip
RUN chmod +x /opt/devkitpro/tools/bin/makerom

# ----- Clean up temporaries -----
WORKDIR /tmp
RUN rm citro3d-wyatt-james.zip
RUN rm makerom.zip
RUN rm -rf citro3d-wyatt-james-temp
RUN rm -rf citro3d-wyatt-james
RUN rm -rf libctru-wyatt-james-temp
RUN rm -rf libctru-wyatt-james

# ----- Set up environment variables -----
ENV PATH="/opt/devkitpro/tools/bin/:/sm64/tools:${PATH}"
ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=/opt/devkitpro/devkitARM
ENV DEVKITPPC=/opt/devkitpro/devkitPPC

# ----- Navigate to final working directory -----
RUN mkdir /sm64
WORKDIR /sm64

# ----- How to build this Dockerfile -----

# Replace <yourname> with your screen name and <yourversion> with anything you'd like. Don't worry, nothing will be uploaded.

# build docker image: `docker build -t <yourname>/sm64:<yourversion> - < ./Dockerfile`
# build SM643DS:      `docker run --rm -v $(pwd):/sm64 <yourname>/sm64:<yourversion> make --jobs 8 VERSION=us`
