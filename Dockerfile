FROM devkitpro/devkitarm:20240202 AS build

RUN mkdir /docker_logs

# ----- Download library archives -----

WORKDIR /tmp

# CREATES makerom.zip
RUN wget https://github.com/3DSGuy/Project_CTR/releases/download/makerom-v0.17/makerom-v0.17-ubuntu_x86_64.zip \
  -O makerom.zip && \
  echo 976c17a78617e157083a8e342836d35c47a45940f9d0209ee8fd210a81ba7bc0  makerom.zip | sha256sum --check

# CREATES citro3d-wyatt-james.zip
RUN wget https://github.com/Wyatt-James/citro3d/archive/91260e180ac94281bed1a40f15ebe97d5fbe1681.zip \
  -O citro3d-wyatt-james.zip && \
  echo 5E505CFF07621095B7E32AFAF146FB47266571A199A5B9C886F4F0D5A235484E  citro3d-wyatt-james.zip | sha256sum --check

# CREATES libctru-wyatt-james.zip
RUN wget https://github.com/Wyatt-James/libctru/archive/44b2b14e41ffde38a26e88b44251ca19ad942639.zip \
  -O libctru-wyatt-james.zip && \
  echo 26FD86982BEF98116648AB7538EEBE80CCE66E346EA99D2E8879737F7C7D7504  libctru-wyatt-james.zip | sha256sum --check

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
RUN make install GPUCMD_DISABLE_BOUNDS_CHECKS=1 GPUCMD_INLINE_THRESH=0 > /docker_logs/make_libctru-wyatt-james.txt
# RUN make install ARCH="-ggdb -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft" > /docker_logs/make_libctru-wyatt-james.txt

# Install wyatt-james's fork of Citro3D. Use the longer line to build with GDB-optimized debug data included.
# Removing this will leave the official devkitPro version installed.
# The profiler defaults to off.
WORKDIR /tmp/citro3d-wyatt-james
RUN make install GPUCMD_DISABLE_BOUNDS_CHECKS=1 GPUCMD_INLINE_THRESH=0 ENABLE_PROFILER=0 > /docker_logs/make_citro3d-wyatt-james.txt
# RUN make install ENABLE_PROFILER=0 > /docker_logs/make_citro3d-wyatt-james.txt
# RUN make install ARCH="-ggdb -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft" > /docker_logs/make_citro3d-wyatt-james.txt

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
