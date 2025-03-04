from threading import Thread
from typing import Union
import subprocess
import glob
import time
import sys
import os

import yaml
from fastapi import FastAPI, Request


with open("/opt/sipcall/config.yaml", "r", encoding="utf8") as file:
    conf = yaml.load(file, Loader=yaml.FullLoader)
print(conf["domain"])


def run_command(command):
    subprocess.Popen(command)

app = FastAPI()

@app.get("/")
def read_root():
    return {"Hello": "World"}


@app.post("/makecall/{phone}")
def read_item(phone: str, request: Request):
    # print(dict(request.query_params))
    desc = request.query_params.get("desc").replace("+", " ")
    audiofile = "/opt/sipcall/audio/play2.wav"
    for file in glob.glob("/opt/sipcall/audio/*.wav"):
        filename = os.path.splitext(os.path.basename(file))[0]
        if filename in desc:
            audiofile = file

    command = ["/bin/sipcall",
        "-sd", conf["domain"],
        "-su", conf["username"],
        "-sp", conf["password"],
        "-so", conf["port"],
        "-p", phone,
        "-s", "1",
        "-r", conf["repeat"],
        "-a", audiofile,
        "-t", conf["timeout"],
    ]
    Thread(target=run_command, args=(command,), daemon=True).start()
    return {"phone": phone, "audiofile": audiofile, "command": command}
