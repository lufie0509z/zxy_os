FROM ubuntu
ENV DEBIAN_FRONTEND=noninteractive

# base dev tools
RUN apt-get update && \
    apt-get install -y sudo build-essential make openssh-client git vim wget curl unzip valgrind pkg-config && \
    rm -rf /var/lib/apt/lists/*

#
RUN sudo apt-get install -y gcc nasm && \
    rm -rf /var/lib/apt/lists/*

#zsh
RUN sudo apt-get install -y zsh && \
    sh -c "$(curl -fsSL https://raw.github.com/ohmyzsh/ohmyzsh/master/tools/install.sh)"

# clear apt cache
RUN rm -rf /var/lib/apt/lists/*
