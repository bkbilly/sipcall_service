FROM python:3.12.9 AS build


RUN apt update
RUN apt install gcc g++ make


WORKDIR /opt/sipcall
COPY Makefile /opt/sipcall
COPY sipcall.c /opt/sipcall

RUN wget https://github.com/pjsip/pjproject/archive/refs/tags/2.15.1.tar.gz -O pjproject.tar.gz
RUN tar -xvf pjproject.tar.gz
RUN cd pjproject-2.15.1

WORKDIR /opt/sipcall/pjproject-2.15.1
RUN ./configure
RUN make dep
RUN make

WORKDIR /opt/sipcall
RUN make


FROM python:3.12.9 AS production
RUN pip install fastapi[standard]
COPY --from=build /opt/sipcall/sipcall /bin/sipcall
WORKDIR /opt/sipcall
COPY main.py /opt/sipcall/


CMD ["/usr/local/bin/fastapi", "run", "main.py"]
