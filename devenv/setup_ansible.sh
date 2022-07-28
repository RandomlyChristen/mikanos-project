#!/bin/sh -ex

ansible-playbook -K -i ansible_inventory ansible_provision.yml