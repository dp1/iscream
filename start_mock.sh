#!/bin/sh

killall broker_mqtts

tmux new-session \; \
  rename-window 'iScream mock setup' \; \
  send-keys 'cd mosquitto.rsmb/rsmb/src' Enter\; \
  send-keys './broker_mqtts config.conf' Enter\; \
  split-window -h -p 50\; \
  send-keys 'python mock/mock_board.py' Enter \; \
  split-window -v -p 50 \; \
  send-keys 'cd webif' Enter\; \
  send-keys 'python server.py' Enter\; \
  select-pane -t 0 \; \
  split-window -v -p 50\; \
  send-keys 'cd bridge' Enter\; \
  send-keys 'python bridge.py' Enter\; \
  select-pane -t 0
