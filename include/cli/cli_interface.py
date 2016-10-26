import os
from copy import copy
from time import sleep

import lib.click as click
from formatter import CliFormatter, JsonFormatter
from utils import name_from_file_path
from veertu_manager import get_veertu_manager, VeertuManagerException, VeertuAppNotFoundException, VMNotFoundException, \
    ImportExportFailedException
import tarfile
import plistlib

veertu_manager = get_veertu_manager()
cli_formatter = CliFormatter()


class CliContext(object):

    def __init__(self):
        self.machine_readable = False
        self.vm_id = None
            

@click.group()
@click.option('--machine-readable', is_flag=True, default=False)
@click.pass_context
def main(ctx, machine_readable):
    veertu_manager.version()
    ctx.obj = CliContext()
    global cli_formatter
    if machine_readable:
        ctx.obj.machine_readable = True
        cli_formatter = JsonFormatter()


@click.command(help='Shows a list of VMs, ids and names')
def list():
    vms_list = veertu_manager.list()
    cli_formatter.format_list_output(vms_list)


@click.command(help='Show runtime VM state and properties. VM can be name or id.')
@click.argument('vm_id')
@click.option('--state', default=False, is_flag=True, help='Show state of vm')
@click.option('--ip-address', default=False, is_flag=True, help='Show ip address of vm')
@click.option('--port-forwarding', default=False, is_flag=True, help='show port forwarding info of vm')
@click.pass_context
def show(ctx, vm_id, state, ip_address, port_forwarding):
    vm_info = veertu_manager.show(vm_id)
    if state:
        click.echo(vm_info.get('status'))
    if ip_address:
        click.echo(vm_info.get('ip'))
    if port_forwarding:
        cli_formatter.format_port_forwarding_info(vm_info.get('port_forwarding', []))
    if any([state, ip_address, port_forwarding]):
        return
    if vm_info.get('status', False) != 'running' and not ctx.obj.machine_readable:
        keep_keys = ['id', 'name', 'status']
        for k, v in vm_info.iteritems():
            if k not in keep_keys:
                del vm_info[k]
    cli_formatter.format_show_output(vm_info)


@click.command(help='Starts or resumes paused VM')
@click.argument('vm_id')
@click.option('--restart', is_flag=True, default=False)
def start(vm_id, restart):
    success = veertu_manager.start(vm_id, restart=restart)
    cli_formatter.format_start_output(success, restart=restart, vm_id=vm_id)


@click.command(help='Pauses a VM')
@click.argument('vm_id')
def pause(vm_id):
    success = veertu_manager.pause(vm_id)
    cli_formatter.format_pause_output(success, vm_id)


@click.command(help='Shuts down a vm')
@click.argument('vm_id')
@click.option('--force', is_flag=True, default=False)
def shutdown(vm_id, force):
    success = veertu_manager.shutdown(vm_id, force=force)
    cli_formatter.format_shutdown_output(success, vm_id)


@click.command(help='Restarts a VM')
@click.argument('vm_id')
@click.option('--force', is_flag=True, default=False)
def reboot(vm_id, force):
    success = veertu_manager.reboot(vm_id, force=force)
    cli_formatter.format_reboot_output(success, vm_id)


@click.command(name='delete', help="Deletes a VM")
@click.argument('vm_id')
@click.option('--yes', is_flag=True, default=False)
@click.pass_context
def delete_vm(ctx, vm_id, yes):
    try:
        vm = veertu_manager.show(vm_id)
    except VMNotFoundException:
        return cli_formatter.format_vm_not_exist()
    if not yes and not ctx.obj.machine_readable:
        click.confirm('are you sure you want to delete vm {} {}'.format(vm['id'], vm['name']), abort=True)
    success = veertu_manager.delete(vm_id)
    cli_formatter.format_delete_output(success, vm_id)


@click.command(help='Exports a vm to a file')
@click.argument('vm_id')
@click.argument('output_file', type=click.Path(exists=False))
@click.option('--fmt', default='vmz', type=click.Choice(['vmz', 'box']), required=False)
@click.option('--silent', is_flag=True, default=False)
@click.pass_context
def export(ctx, vm_id, output_file, fmt, silent):
    try:
        if ctx.obj.machine_readable:
            veertu_manager.export_vm(vm_id, output_file, fmt=fmt, silent=silent)
            return
        if os.path.isfile(output_file):
            click.confirm('File exists, do you want to overwrite?', default=False, abort=True)

        handle = veertu_manager.export_vm(vm_id, output_file, fmt=fmt, silent=silent, do_progress_loop=False)
        if not handle:
            return click.echo('could not export vm')
        if fmt == 'vmz':
            length = 100
        else:
            length = 200
        _do_import_export_progress_bar(handle, length)
    except ImportExportFailedException:
        raise ImportExportFailedException("export failed")


def _do_import_export_progress_bar(handle, length):
    with click.progressbar(length=length, show_eta=False) as bar:
        progress = 0
        previous_progress = 0
        while progress < length:
            progress = veertu_manager.progress(handle)
            step = progress - previous_progress
            if step == 0:
                sleep(1)
            previous_progress = copy(progress)
            bar.update(step)


@click.command(name='import', help='Import a vm into Veertu')
@click.argument('input_file', type=click.Path(exists=True))
@click.option('--os-family', type=click.STRING)
@click.option('--os-type', type=click.STRING)
@click.option('--name', type=click.STRING)
@click.option('--fmt', type=click.STRING)
@click.option('-n', is_flag=True, default=False)
@click.pass_context
def import_vm(ctx, input_file, os_family, os_type, name, fmt, n):
    if n:
        suggested_name = _try_guess_name(input_file)
        return click.echo('Suggested VM name "%s"' % suggested_name)
    try:
        if ctx.obj.machine_readable:
            silent = True
            success = veertu_manager.import_vm(input_file, name, os_family, os_type, fmt,
                                               silent=silent, do_progress_loop=True)
            return cli_formatter.echo_status_ok() if success else cli_formatter.echo_status_failure()
        else:
            handle = veertu_manager.import_vm(input_file, name, os_family, os_type, fmt, do_progress_loop=False)
            if not handle:
                return cli_formatter.echo_status_failure(message='could not import vm')
            _do_import_export_progress_bar(handle, 100)
    except ImportExportFailedException:
        raise ImportExportFailedException("Veertu failed to import %s, "
                                          "please check file type and try again" % input_file)


def _try_guess_name(file_path):
    try:  # if this is a box file try to open it with gzip
        with tarfile.open(file_path, 'r:gz') as tf:
            d = {}
            for info in tf:
                if info.name == 'settings.plist':
                    d = plistlib.readPlist(tf.extractfile(info))
                    break
            suggested_name = d.get('name', d.get('display_name', None))
            if not suggested_name:
                return name_from_file_path(file_path)
            return suggested_name
    except tarfile.ReadError:  # file is not a box file so we guess name according to file name
        return name_from_file_path(file_path)

@click.command(name='create')
@click.argument('input_file', type=click.Path(exists=True))
@click.option('--os-family', type=click.STRING)
@click.option('--os-type', type=click.STRING)
@click.option('--name', type=click.STRING)
@click.pass_context
def create_vm(ctx, input_file, os_family, os_type, name):
    new_uuid = veertu_manager.create_vm(input_file, name, os_family, os_type)
    cli_formatter.format_create(new_uuid)
    if new_uuid and not ctx.obj.machine_readable:
        if click.confirm('Would you like to start the new vm?', default=False):
            veertu_manager.start(new_uuid)


@click.command(help='Show all data for a VM')
@click.argument('vm_id')
def describe(vm_id):
    vm_dict = veertu_manager.describe(vm_id)
    cli_formatter.format_describe(vm_dict)


@click.group(help='Modifys a VM settings')
@click.argument('vm_id')
@click.pass_context
def modify(ctx, vm_id):
    ctx.obj.vm_id = vm_id


@modify.command()
@click.pass_context
@click.option('--headless', type=click.Choice(['0', '1']), default=None, required=False,
              help='set headless mode 1 for on or 0 for off')
@click.option('--name', default=None, help="set new name for the VM")
@click.option('--cpu', default=None, help="set number of cpu cores")
@click.option('--ram', default=None, help="set memory - [0-9999]MB or [0-9]GB")
@click.option('--read-only', type=click.Choice(['0', '1']), default=None, help="set vm read only 1 for on or 0 for off")
@click.option('--hdpi', type=click.Choice(['0', '1']), default=None, help="set hdpi support 1 for on or 0 for off")
@click.option('--copy-paste', type=click.Choice(['0', '1']), default=None,
              help="set copy paste support 1 for on or 0 for off")
@click.option('--file-sharing', type=click.Choice(['0', '1']), default=None,
              help="set file sharing 1 for on or 0 for off")
@click.option('--network', default=None, help="choose network card to modify")
@click.option('--network-type', default=None, type=click.Choice(['shared', 'host', 'disconnected']),
              help="set network type")
def set(ctx, headless, name, cpu, ram, read_only, hdpi, copy_paste, file_sharing, network, network_type):
    vm_id = ctx.obj.vm_id
    good_ones = {}
    bad_ones = {}
    if headless:
        success = veertu_manager.set_headless(vm_id, headless)
        _add_to_dict(success, 'headless', headless, good_ones=good_ones, bad_ones=bad_ones)
    if name:
        success = veertu_manager.rename(vm_id, name)
        _add_to_dict(success, 'name', name, good_ones=good_ones, bad_ones=bad_ones)
    if cpu:
        success = veertu_manager.set_cpu(vm_id, cpu)
        _add_to_dict(success, 'cpu', cpu, good_ones=good_ones, bad_ones=bad_ones)
    if ram:
        success = veertu_manager.set_ram(vm_id, ram)
        _add_to_dict(success, 'ram', ram, good_ones=good_ones, bad_ones=bad_ones)
    if network and network_type:
        success = veertu_manager.set_network_type(vm_id, network, network_type)
        _add_to_dict(success, 'network type', network_type, good_ones=good_ones, bad_ones=bad_ones)
    cli_formatter.format_properties_changed(good_ones, bad_ones)


def _add_to_dict(success, key, value, good_ones={}, bad_ones={}):
    if success:
        good_ones[key] = value
    else:
        bad_ones[key] = value


@modify.group()
def add():
    pass


@add.command(name='port_forwarding')
@click.pass_context
@click.argument('rule_name')
@click.option('--host-ip', default=None)
@click.option('--host-port', default=None)
@click.option('--guest-ip', default=None)
@click.option('--guest-port', default=None)
@click.option('--protocol', default=None)
def add_port_forwarding(ctx, rule_name, host_ip, host_port, guest_ip, guest_port, protocol):
    vm_id = ctx.obj.vm_id
    success = veertu_manager.add_port_forwarding(vm_id, rule_name, host_ip, host_port, guest_ip, guest_port,
                                                 protocol=protocol)
    cli_formatter.format_added_port_forwarding_rule(success)


@add.command(name="network_card")
@click.pass_context
@click.option('--type', type=click.Choice(['shared', 'host', 'disconnected']), default='shared')
@click.option('--model', type=click.Choice(['e1000', 'rtl8139']), default='e1000')
def add_network_card(ctx, type, model):
    success = veertu_manager.add_network_card(ctx.obj.vm_id, type, model)
    cli_formatter.format_add_network_card(success)

@modify.group()
def delete():
    pass


@delete.command('port_forwarding')
@click.pass_context
@click.argument('rule_name')
def delete_port_forwarding(ctx, rule_name):
    vm_id = ctx.obj.vm_id
    success = veertu_manager.remove_port_forwarding(vm_id, rule_name)
    cli_formatter.format_deleted_port_forwarding_rule(success)


@delete.command('network_card')
@click.pass_context
@click.argument('card_index')
def delete_network_card(ctx, card_index):
    vm_id = ctx.obj.vm_id
    success = veertu_manager.delete_network_card(vm_id, card_index)
    cli_formatter.format_delete_network_card(success)


main.add_command(list)
main.add_command(show)
main.add_command(start)
main.add_command(pause)
main.add_command(shutdown)
main.add_command(reboot)
main.add_command(delete_vm)
main.add_command(export)
main.add_command(import_vm)
main.add_command(describe)
main.add_command(modify)
main.add_command(create_vm)

if __name__ == '__main__':
    # import sys
    # print (sys.argv[1:])
    # exit()
    try:
        main()
    except VeertuAppNotFoundException:
        cli_formatter.echo_status_failure(message='Veertu app not found please check configuration')
    except VeertuManagerException as e:
        cli_formatter.echo_status_failure(message=e.message)

