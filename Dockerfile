FROM devkitpro/devkitarm:20240202 AS build

RUN mkdir /docker_logs

# ----- Download library archives -----

WORKDIR /tmp

# CREATES makerom.zip
RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip \
  -O makerom.zip && \
  echo 976c17a78617e157083a8e342836d35c47a45940f9d0209ee8fd210a81ba7bc0  makerom.zip | sha256sum --check

# CREATES citro3d-derrekr.zip
RUN wget https://github.com/derrekr/citro3d/archive/71f33882fb4e1e7ccda455ea187c1fab6dec64d4.zip \
  -O citro3d-derrekr.zip && \
  echo 4f36dfb3b0c41a1ea9161c57448674a6052bd84962210ecf99f0ffbd5a7af3d9  citro3d-derrekr.zip | sha256sum --check

# ----- Extract archives in-place, removing commit-specific container folders -----
  
# CREATES citro3d-derrekr-temp, citro3d-derrekr
RUN unzip -d ./citro3d-derrekr-temp citro3d-derrekr.zip
RUN mv ./citro3d-derrekr-temp/citro3d-* ./citro3d-derrekr

# ----- Install dependencies -----

# Install derrekr's fork of Citro3D. Use the longer line to build with GDB-optimized debug data included.
# Removing this will leave the official devkitPro version installed.
WORKDIR /tmp/citro3d-derrekr
RUN make install > /docker_logs/make_citro3d-derrekr.txt
# RUN make install ARCH="-ggdb -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft" > /docker_logs/make_citro3d-derrekr.txt

# Install makerom
WORKDIR /tmp
RUN unzip -d /opt/devkitpro/tools/bin/ makerom.zip
RUN chmod +x /opt/devkitpro/tools/bin/makerom

# ----- Clean up temporaries -----
WORKDIR /tmp
RUN rm citro3d-derrekr.zip
RUN rm makerom.zip
RUN rm -rf citro3d-derrekr-temp
RUN rm -rf citro3d-derrekr

# ----- Set up environment variables -----
ENV PATH="/opt/devkitpro/tools/bin/:/sm64/tools:${PATH}"
ENV DEVKITPRO=/opt/devkitpro
ENV DEVKITARM=/opt/devkitpro/devkitARM
ENV DEVKITPPC=/opt/devkitpro/devkitPPC

# ----- Install Misc. Packages (WYATT_TODO unnecessary?) -----
RUN apt-get update && \
apt-get install -y \
  binutils-mips-linux-gnu \
  bsdmainutils \
  build-essential \
  libaudiofile-dev \
  pkg-config \
  python3 \
  wget \
  unzip \
  zlib1g-dev

# ----- Navigate to final working directory -----
RUN mkdir /sm64
WORKDIR /sm64

# ----- How to build this Dockerfile -----

# Replace <yourname> with your screen name and <yourversion> with anything you'd like. Don't worry, nothing will be uploaded.

# build docker image: `docker build -t <yourname>/sm64:<yourversion> - < ./Dockerfile`
# build SM643DS:      `docker run --rm -v $(pwd):/sm64 <yourname>/sm64:<yourversion> make --jobs 8 VERSION=us`
