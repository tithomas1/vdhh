
import os
import ConfigParser
import subprocess
import uuid
from collections import OrderedDict
import tarfile
import time
import shutil
import plistlib


class VeertuManager(object):

    def __init__(self):
        self.app = "Veertu"
        parser = ConfigParser.SafeConfigParser()
        cfg_file = os.path.expanduser('~/.veertu_config')
        if os.path.exists(cfg_file):
            parser.read(cfg_file)
            if parser.get('DEFAULT', 'APP_PATH'):
                self.app = parser.get('DEFAULT', 'APP_PATH')
                return
        self.app = self._find_app_name(['Veertu', 'Veertu 2016 Business'])
        
        
    def _find_app_name(self, options):
        tmp_app = self.app
        for app in options:
            try:
                self.app = app
                self.version()
                return app
            except VeertuAppNotFoundException:
                continue
        return tmp_app
                

    def _call_veertu_app(self, command, *args, **kwargs):
        if kwargs.get('format', True):
            command = command.format(*args)
        # print command
        osscript_output  = subprocess.check_output([
            'osascript', '-e', 'tell application "%s" to ' % self.app + command], stderr=subprocess.PIPE).strip()
        if kwargs.get('return_as_dict', kwargs.get('return_list_of_dicts', False)):
            projection = kwargs.get('projection', [])  # this works a bit crooked because the app returns list of
            return_list_of_dicts = kwargs.get('return_list_of_dicts', False)  # objects as {id, id, name, name}
            list_output = self._split_and_strip(osscript_output)
            projection_length = len(projection)
            list_output_length = len(list_output)
            if list_output_length % projection_length != 0:
                raise WrongProjectionException('wrong parameters passed to projection')
            objects_returned = list_output_length / projection_length
            if objects_returned == 1 and not return_list_of_dicts:  # if there is only one object return it as a dict
                return self._turn_into_dict(list_output, projection)  # unless there was a request for multiple results
            output_lists = [OrderedDict() for i in range(0, objects_returned)]
            for key, is_return in projection.iteritems():
                for i in range(0, objects_returned):  # distribute each item in the list to it's suitable dict
                    item = list_output.pop(0)
                    if is_return:
                        output_lists[i][key] = item
            return output_lists
        osscript_output = osscript_output.strip()
        if kwargs.get('scalar', False):
            if self._is_int_parsed(osscript_output):
                return bool(int(osscript_output))
            if osscript_output.lower() == 'true':
                return True
            return False

        if kwargs.get('number', False):
            try:
                return int(osscript_output)
            except ValueError:
                raise InternalAppError('There was an internal error, your process might have succeeded. please check')

        if kwargs.get('return_formatted', True):
            return self._split_and_strip(osscript_output)

        return osscript_output

    def _is_int_parsed(self, num):
        try:
            i = int(num)
            return True
        except ValueError:
            return False

    def _turn_into_dict(self, list_output, projection):
        output_dict = OrderedDict()
        for key, is_return in projection.iteritems():
            item = list_output.pop(0)
            if is_return:
                output_dict[key] = item
        return output_dict


    def _call_veertu_with_name_fallback(self, command, *args, **kwargs):
        """
        wrapper for _call_veertu_app method for automating find by name fallback (optimistic)
        :param command: the command string (just as you'd pass to _call_veertu_app)
        :param args: the command args (just as you'd pass to _call_veertu_app)
        :param kwargs: kwargs are passed to _call_veertu_app

            pass the vm_id in the 'id_value' named argument for the fallback to work

        :return:
        """
        id_value = kwargs.pop('id_value', args[0])
        command_formatted = command.format(*args)
        try:
            return self._call_veertu_app(command_formatted, format=False, **kwargs)
        except subprocess.CalledProcessError as e:
            vms_list = self.list()
            for vm in vms_list:
                name = vm.get('name')
                if name == id_value:
                    vm_id = vm.get('id')
                    return self._call_veertu_app(command_formatted.replace(id_value, vm_id), format=False, **kwargs)
        raise VMNotFoundException("vm %s was not found" % id_value)

    def list(self):
        projection_args = OrderedDict([('id', True), ('name', True)])
        return self._call_veertu_app('{{id, name}} of every vm', return_list_of_dicts=True, projection=projection_args)

    def show(self, vm_id, state=True, ip_address=True, port_forwarding=True):
        projection_args = OrderedDict([('id', True), ('name', True), ('status', state), ('ip', ip_address)])
        command = 'get {{id, name, status, ip}} of vm id "{}"'
        vm_info = self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, return_as_dict=True,
                                                       projection=projection_args)
        if port_forwarding:
            vm_info['port_forwarding'] = self.get_port_forwarding(vm_info.get('id'))
        return vm_info

    def get_port_forwarding(self, vm_id, protocol=True, description=True, host_ip=True,
                            host_port=True, guest_ip=True, guest_port=True):
        projection_args = OrderedDict([('name', True), ('protocol', protocol),
                                       ('host_ip', host_ip), ('host_port', host_port),
                                       ('guest_ip', guest_ip), ('guest_port', guest_port)])
        command = '{{name, protocol, host ip, host port, guest ip, guest port}}' \
                  ' of port forwarding of advanced settings of vm id "{}"'
        port_forwarding_info = self._call_veertu_app(command, vm_id, return_as_dict=True, projection=projection_args,
                                                     return_list_of_dicts=True)
        for idx, port_forwarding_dict in enumerate(port_forwarding_info):  # clean any empty rules
            if not any(port_forwarding_dict.values()):
                del port_forwarding_info[idx]
        if description:
            for port_forwarding_dict in port_forwarding_info:
                rule_description = self.get_port_forwarding_description(vm_id, port_forwarding_dict.get('name', ''))
                port_forwarding_dict['description'] = rule_description
        return port_forwarding_info

    def get_port_forwarding_description(self, vm_id, rule_name):
        command = 'virtualbox description of port forwarding "{}" of advanced settings of vm id "{}"'
        return self._call_veertu_app(command, rule_name, vm_id, return_formatted=False)

    def start(self, vm_id, restart=False):
        if restart:
            return self.reboot(vm_id)
        command = 'start of vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def pause(self, vm_id):
        command = 'suspend of vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def shutdown(self, vm_id, force=False):
        if force:
            command = 'force shutdown of vm id "{}"'
        else:
            command = 'shutdown of vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def reboot(self, vm_id, force=False):
        if force:
            self.shutdown(vm_id, force=True)
            return self.start(vm_id)
        command = 'restart   of vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def delete(self, vm_id):
        command = 'delete vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def export_vm(self, vm_id, output_file, fmt='box', silent=False, do_progress_loop=True):
        if not output_file:
            raise NoOutputFileSpecified('no output file specified')
        d, f = os.path.split(output_file)
        if '.' not in f:
            f += "." + fmt
        output_file = os.path.join(d, f)

        command = 'export vm id "{}" to POSIX file "{}" format "{}"'
        handle = self._call_veertu_with_name_fallback(command, vm_id, output_file, fmt, id_value=vm_id,
                                                      return_formatted=False)
        if not handle:
            return False
        if do_progress_loop:
            self.progress_loop(handle, silent=silent)
            return True
        return handle

    def create_vm(self, file_path, name, os_family, os_type):
        command = 'create vm POSIX file "{}" with name "{}"  os "{}"  os family "{}"'
        if not name:
            d, f = os.path.split(file_path)
            name = f
            if '.' in name:
                name = name.split('.').pop(0)
        result = self._call_veertu_app(command, file_path, name, os_type, os_family).pop()
        if result == 'false':
            return None
        return result

    def import_vm(self, file_path, name, os_family, os_type, fmt, silent=False, do_progress_loop=True):
        command = 'import vm POSIX file "{}" with name "{}" '
        if not name:
            d, f = os.path.split(file_path)
            name = f
            if '.' in name:
                name = name.split('.').pop(0)
        args = [file_path, name]
        if os_type:
            command += ' os "{}" '
            args.append(os_type)
        if os_family:
            command += ' os family "{}"'
            args.append(os_family)

        handle = self._call_veertu_app(command, *args, return_formatted=False)
        if do_progress_loop:
            self.progress_loop(handle, silent=silent)
            return True
        return handle

    def progress_loop(self, handle, silent=False):
        progress = ''
        while progress.strip() != '1.00':
            progress = self._call_veertu_app('get progress of "{}"', handle, return_formatted=False)
            if not progress or progress == '(null)':
                raise ImportExportFailedException("process failed to complete")
            if not silent:
                print(progress)
            if progress.strip() == '2.00':
                break
            time.sleep(1)

    def progress(self, handle):
        progress_string = self._call_veertu_app('get progress of "{}"', handle, return_formatted=False)
        if not progress_string or progress_string == '(null)':
            raise ImportExportFailedException("process failed to complete")
        progress_num = int(float(progress_string.strip()) * 100)
        if progress_num == 0:
            return 1
        return progress_num

    def describe(self, vm_id, advanced_settings=True, general_settings=True, hardware=True):
        keys = ['id', 'name', 'status', 'ip', 'version']
        vm_info = self._get_section(vm_id, keys)
        if advanced_settings:
            vm_info['advanced_settings'] = self.get_advanced_settings(vm_info.get('id'))
        if general_settings:
            vm_info['general_settings'] = self.get_general_settings(vm_info.get('id'))
        if hardware:
            vm_info['hardware'] = self.get_hardware(vm_info.get('id'))
        return vm_info

    def _get_section(self, vm_id, keys, section=''):
        projection_args = OrderedDict([(k, True) for k in keys])
        section_string = ''
        if section:
            if isinstance(section, basestring):
                section_string = "of " + section
            if isinstance(section, list):
                section_string = ' '.join(["of " + exp for exp in section])
        command = 'get {{' + ', '.join(keys) + '}} ' + section_string + ' of vm id "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id,
                                                    return_as_dict=True, projection=projection_args)


    def get_advanced_settings(self, vm_id):
        advanced_settings = self._get_section(vm_id, ['snapshot', 'headless', 'hdpi', 'remap cmd'], 'advanced settings')
        advanced_settings['port_forwarding'] = self.get_port_forwarding(vm_id, protocol=True, description=True,
                                                                        host_ip=True, host_port=True, guest_ip=True,
                                                                        guest_port=True)
        guest_tools = self.get_guest_tools(vm_id)
        advanced_settings.update(guest_tools)
        return advanced_settings

    def get_guest_tools(self, vm_id):
        keys = ['file sharing', 'copy paste', 'shared folder']
        return self._get_section(vm_id, keys, ['guest tools', 'advanced settings'])

    def get_hardware(self, vm_id):
        keys = ['chipset', 'ram', 'acpi', 'hpet', 'hyperv', 'vga']
        hardware_info = self._get_section(vm_id, keys, 'hardware')
        hardware_info['hardisks'] = self.get_harddisks(vm_id)
        hardware_info['audio'] = self.get_audio(vm_id)
        hardware_info['cd roms'] = self.get_cd_rom(vm_id)
        hardware_info['disk controllers'] = self.get_disk_controller(vm_id)
        hardware_info['network cards'] = self.get_network_cards(vm_id)
        return hardware_info

    def get_harddisks(self, vm_id):
        keys = ['drive index', 'boot', 'controller', 'bus', 'file', 'size']
        hardisks = self._get_section(vm_id, keys, ['hardisks', 'hardware'])
        return hardisks

    def get_audio(self, vm_id):
        keys = ['audio index', 'type']
        return self._get_section(vm_id, keys, ['audio', 'hardware'])

    def get_cd_rom(self, vm_id):
        keys = ['cd index', 'cd controller', 'cd file', 'cd bus', 'cd type', 'media in']
        return self._get_section(vm_id, keys, ['cd rom', 'hardware'])

    def get_disk_controller(self, vm_id):
        keys = ['controller index', 'controller type', 'controller model', 'controller mode']
        return self._get_section(vm_id, keys, ['disk controller', 'hardware'])

    def get_network_cards(self, vm_id):
        keys = ['card index', 'connection', 'pci bus', 'mac address', 'card model', 'card family']
        return self._get_section(vm_id, keys, ['network card', 'hardware'])

    def get_general_settings(self, vm_id):
        keys = ['os', 'os family', 'boot device']
        projection_args = OrderedDict([(k, True) for k in keys])
        command = 'get {{' + ', '.join(keys) + '}} of general settings of vm id "{}"'
        general_settings = self._call_veertu_app(command, vm_id, return_as_dict=True, projection=projection_args)
        return general_settings

    def set_headless(self, vm_id, value):
        if value:
            command = 'set headless vm id "{}" to true'
        else:
            command = 'set headless vm id "{}" to false'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def unset_headless(self, vm_id):
        command = 'set headless vm id "{}" to false'
        return self._call_veertu_with_name_fallback(command, vm_id, id_value=vm_id, scalar=True)

    def add_port_forwarding(self, vm_id, name, host_ip, host_port, guest_ip, guest_port, protocol='tcp'):
        command = 'listen on "{}" {} port {} forward to vm id "{}" port {} with name "{}"'
        return self._call_veertu_with_name_fallback(command,host_ip, protocol, host_port, vm_id,
                                                    guest_port, name, id_value=vm_id, number=True)

    def remove_port_forwarding(self, vm_id, rule_name):
        command = 'remove port forwarding "{}" from vm id "{}"'
        return self._call_veertu_with_name_fallback(command, rule_name, vm_id, id_value=vm_id, number=True)

    def rename(self, vm_id, new_name):
        command = 'rename vm id "{}" to name "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, new_name, id_value=vm_id, return_formatted=False)

    def set_cpu(self, vm_id, cpu_count):
        return self.set_property(vm_id, 'cpu count', cpu_count)

    def set_ram(self, vm_id, ram):
        return self.set_property(vm_id, 'ram', '"%s"' % ram)

    def set_network_type(self, vm_id, card_index, type):
        command = 'set network card connection type of vm id "{}" with index {} to "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, card_index, type, id_value=vm_id, scalar=True)

    def add_network_card(self, vm_id, connection_type, model):
        command = 'add network card vm id "{}" connection type "{}" model "{}"'
        return self._call_veertu_with_name_fallback(command, vm_id, connection_type, model,
                                                    id_value=vm_id, scalar=True)

    def delete_network_card(self, vm_id, card_index):
        command = 'remove network card vm id "{}" index {}'
        return self._call_veertu_with_name_fallback(command, vm_id, card_index, id_value=vm_id, scalar=True)

    def set_property(self, vm_id, property, value, string_type=False, **kwargs):
        if string_type:
            command = 'set {} vm id "{}" to "{}"'
        else:
            command = 'set {} vm id "{}" to {}'
        oargs = {'scalar': True}
        oargs.update(kwargs)
        return self._call_veertu_with_name_fallback(command, property, vm_id, value, id_value=vm_id, **oargs)

    def get_property(self, vm_id, property, section):
        command = 'get {} of {} of vm id "{}"'
        return self._call_veertu_app(command, property, section, vm_id, return_formatted=False)

    def version(self):
        command = 'version'
        try:
            response = self._call_veertu_app(command)
            if response:
                return True
        except subprocess.CalledProcessError:  # in case osscript return error
            raise VeertuAppNotFoundException('could not get version')
        return False

    @classmethod
    def _split_and_strip(cls, str_list):
        return [item.strip().replace('missing value', '-') for item in str_list.split(',')]


class VeertuManagerException(Exception):
    pass


class VMNotFoundException(VeertuManagerException):
    pass


class NoOutputFileSpecified(VeertuManagerException):
    pass


class InternalAppError(VeertuManagerException):
    pass


class ImportExportFailedException(VeertuManagerException):
    pass

class WrongProjectionException(Exception):
    pass

class VeertuAppNotFoundException(VeertuManagerException):
   pass


class VeertuManager120(VeertuManager):
    pass


def get_veertu_manager(version='1.2.0'):
    return VeertuManager120()
