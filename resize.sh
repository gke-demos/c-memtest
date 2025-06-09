#!/bin/sh

kubectl patch pod c-memtest --subresource resize --patch \
  '{"spec":{"containers":[{"name":"c-memtest", "resources":{"requests":{"memory":"2G"},"limits":{"memory":"2G"}}}]}}'