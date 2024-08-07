FROM ubuntu:20.04

LABEL image_label="micro2024_521" \
    org.opencontainers.image.title="micro2024_521_controller" \
    org.opencontainers.image.description="Slurm controller with compiled and usable version of gem5" 


RUN apt -y update && apt -y upgrade \
    && apt -y install \
       wget \
       bzip2 \
       perl \
       gcc \
       git \
       gnupg \
       make \
       munge \
       libmunge-dev \
       python3-dev \
       python3-pip \
       python3 \
       mariadb-server \
       libmariadb-dev \
       psmisc \
       bash-completion \
       libhttp-parser-dev \
       libjson-c-dev

ENV DEBIAN_FRONTEND=noninteractive
RUN apt -y update && apt -y upgrade && \
        apt -y install \
        build-essential \
        git \
        m4 \
        scons \
        zlib1g \
        zlib1g-dev \
        libprotobuf-dev \
        protobuf-compiler \
        libprotoc-dev \
        libgoogle-perftools-dev \
        python3-dev \
        python-is-python3 \
        doxygen \
        libboost-all-dev \
        libhdf5-serial-dev \
        python3-pydot \
        libpng-dev \
        libelf-dev \
        pkg-config \
        pip \
        python3-venv black \
        qemu-kvm \
        libvirt-daemon-system \
        libvirt-clients \
        bridge-utils \
        libhdf5-dev \
        kmod

RUN pip3 install Cython nose mypy pre-commit

ARG GOSU_VERSION=1.11

RUN set -ex \
    && wget -O /usr/local/bin/gosu "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-amd64" \
    && wget -O /usr/local/bin/gosu.asc "https://github.com/tianon/gosu/releases/download/$GOSU_VERSION/gosu-amd64.asc" \
    && export GNUPGHOME="$(mktemp -d)" \
    && gpg --batch --keyserver hkps://keys.openpgp.org --recv-keys B42F6819007F00F88E364FD4036A9C25BF357DD4 \
    && gpg --batch --verify /usr/local/bin/gosu.asc /usr/local/bin/gosu \
    && rm -rf "${GNUPGHOME}" /usr/local/bin/gosu.asc \
    && chmod +x /usr/local/bin/gosu \
    && gosu nobody true

ARG SLURM_TAG=slurm-21-08-6-1

RUN set -x \
    && git clone -b ${SLURM_TAG} --single-branch --depth=1 https://github.com/SchedMD/slurm.git \
    && cd slurm \
    && ./configure --enable-debug --prefix=/usr --sysconfdir=/etc/slurm-llnl \
        --with-mysql_config=/usr/bin  --libdir=/usr/lib64 \
    && make install \
    && install -D -m644 etc/cgroup.conf.example /etc/slurm-llnl/cgroup.conf.example \
    && install -D -m644 etc/slurm.conf.example /etc/slurm-llnl/slurm.conf.example \
    && install -D -m644 etc/slurmdbd.conf.example /etc/slurm-llnl/slurmdbd.conf.example \
    && install -D -m644 contribs/slurm_completion_help/slurm_completion.sh /etc/profile.d/slurm_completion.sh \
    && cd .. \
    && rm -rf slurm \
    && groupadd -r --gid=990 slurm \
    && useradd -r -g slurm --uid=990 slurm \
    && mkdir /var/spool/slurmd \
        /var/run/slurmd \
        /var/run/slurmdbd \
        /var/lib/slurmd \
        /var/log/slurm \
        /data \
    && touch /var/lib/slurmd/node_state \
        /var/lib/slurmd/front_end_state \
        /var/lib/slurmd/job_state \
        /var/lib/slurmd/resv_state \
        /var/lib/slurmd/trigger_state \
        /var/lib/slurmd/assoc_mgr_state \
        /var/lib/slurmd/assoc_usage \
        /var/lib/slurmd/qos_usage \
        /var/lib/slurmd/fed_mgr_state \
    && chown -R slurm:slurm /var/*/slurm* \
    && /sbin/create-munge-key

RUN mkdir -p /run/munge \
    && chown munge:munge /run/munge \
    && chmod 755 /run/munge

COPY dockerfiles/slurm.conf /etc/slurm-llnl/slurm.conf
COPY dockerfiles/slurmdbd.conf /etc/slurm-llnl/slurmdbd.conf
RUN set -x \
    && chown slurm:slurm /etc/slurm-llnl/slurmdbd.conf \
    && chmod 600 /etc/slurm-llnl/slurmdbd.conf

WORKDIR /gem5
COPY . /gem5
RUN ./tasks-gem5 build

COPY dockerfiles/docker-entrypoint.sh /usr/local/bin/docker-entrypoint.sh
ENTRYPOINT ["/usr/local/bin/docker-entrypoint.sh"]

CMD ["slurmdbd"]