import json
from collections import OrderedDict

import lib.click as click
from lib.tabulate.tabulate import tabulate


class AbstractFormatter(object):

    def format_list_output(self, vms_list):
        pass

    def format_show_output(self, vm_info):
        pass

    def format_start_output(self, result, restart=False):
        pass

    def format_pause_output(self, result, vm_id):
        pass

    def format_shutdown_output(self, result, vm_id):
        pass

    def format_reboot_output(self, result, vm_id):
        pass

    def format_delete_output(self, result, vm_id):
        pass

    def format_vm_not_exist(self):
        pass

    def echo_status_ok(self):
        pass

    def echo_status_failure(self):
        pass

    def format_properties_changed(self, succeeded, failed):
        pass

    def format_added_port_forwarding_rule(self, result):
        pass

    def format_deleted_port_forwarding_rule(self, result):
        pass

    def format_describe(self, vm_dict):
        pass

    def format_create(self, success):
        pass

    def format_add_network_card(self, success):
        pass

    def format_delete_network_card(self, success):
        pass


class CliFormatter(AbstractFormatter):

    def format_list_of_dicts(self, list_of_dicts):
        if isinstance(list_of_dicts, list) and len(list_of_dicts) > 0:
            headers = dict(zip(list_of_dicts[0].keys(), list_of_dicts[0].keys()))
            return tabulate(list_of_dicts, headers=headers, tablefmt='grid')

    def format_dict(self, dict_to_output):
        output = ''
        data = []
        additionals = {}
        for k, v in dict_to_output.iteritems():
            if isinstance(v, basestring):
                data.append((k, v))
            elif isinstance(v, (dict, OrderedDict)):
                additionals[k] = v
            elif isinstance(v, list):
                if len(v) > 0 and isinstance(v[0], (dict, OrderedDict)):
                    additionals[k] = self.format_list_of_dicts(v)
                else:
                    data.append((k, ', '.join(v)))
        output += tabulate(data, tablefmt='grid')
        output += '\n\n'
        for k, v in additionals.iteritems():
            output += k + "\n\n"
            if isinstance(v, dict):
                output += self.format_dict(v)
            else:
                output += v
            output += '\n\n'
        return output

    def format_list_output(self, vms_list):
        click.echo('list of vms:')
        output = self.format_list_of_dicts(vms_list)
        click.echo(output)

    def format_show_output(self, vm_info):
        output = self.format_dict(vm_info)
        click.echo(output)

    def format_port_forwarding_info(self, info):
        click.echo(self.format_list_of_dicts(info))

    def format_start_output(self, result, restart=False, vm_id=None):
        if result:
            if restart:
                click.echo("VM %s successfully restarted" % vm_id)
            else:
                click.echo("VM %s successfully started" % vm_id)
        else:
            if restart:
                click.echo("VM %s failed to restart" % vm_id, err=True)
            else:
                click.echo("VM %s failed to start" % vm_id, err=True)

    def format_pause_output(self, result, vm_id):
        if result:
            click.echo("VM %s paused" % vm_id)
        else:
            click.echo("VM %s failed to pause" % vm_id, err=True)

    def format_shutdown_output(self, result, vm_id):
        if result:
            click.echo("VM %s is shutting down" % vm_id)
        else:
            click.echo("VM %s was unable to shut down (you can try with --force)" % vm_id, err=True)

    def format_reboot_output(self, result, vm_id):
        if result:
            click.echo("VM %s is rebooting" % vm_id)
        else:
            click.echo("VM %s was unable to reboot (you can try with --force)" % vm_id, err=True)

    def format_delete_output(self, result, vm_id):
        if result:
            click.echo("VM %s deleted successfully" % vm_id)
        else:
            click.echo("Unable to delete VM %s " % vm_id, err=True)

    def format_vm_not_exist(self):
        click.echo('vm does not exist')

    def echo_status_ok(self, message=''):
        click.echo('OK')
        if message:
            click.echo(message)

    def echo_status_failure(self, message=''):
        click.echo('Action Failed')
        if message:
            click.echo(message)

    def format_properties_changed(self, succeeded, failed):
        if succeeded:
            click.echo("the following properties were set successfully:")
            for k, v in succeeded.iteritems():
                click.echo("{} set to {}".format(k, str(v)))
        if failed:
            click.echo('the following properties failed to set:')
            for k, v in failed.iteritems():
                click.echo("{} to {}".format(k, str(v)))

    def format_added_port_forwarding_rule(self, result):
        if result:
            click.echo('rule added successfully')
        else:
            click.echo('could not add port forwarding', err=True)

    def format_deleted_port_forwarding_rule(self, result):
        if result:
            click.echo('rule deleted successfully')
        else:
            click.echo('could not delete port forwarding', err=True)

    def format_describe(self, vm_dict):
        click.echo(self.format_dict(vm_dict))

    def format_create(self, success):
        if success:
            click.echo('vm created successfully new uuid: %s' % success)
        else:
            click.echo('could not create vm')

    def format_add_network_card(self, success):
        if success:
            click.echo('successfully added network card')
        else:
            click.echo('could not add network card')

    def format_delete_network_card(self, success):
        if success:
            click.echo('successfully deleted network card')
        else:
            click.echo('could not delete network card')


class JsonFormatter(AbstractFormatter):

    def _make_response(self, status="OK", body={}, message=''):
        response = {
            'status': status,
            'body': body,
            'message': message
        }
        return response

    def _format_to_json(self, response):
        return json.dumps(response)

    def echo_response(self, body={}, status='OK', message='', err=False):
        if err:
            status = "ERROR"
        response_dict = self._make_response(status=status, body=body, message=message)
        click.echo(self._format_to_json(response_dict))

    def format_list_output(self, vms_list):
        self.echo_response(vms_list)

    def format_show_output(self, vm_info):
        self.echo_response(vm_info)

    def format_port_forwarding_info(self, info):
        self.echo_response(info)

    def format_start_output(self, result, restart=False, vm_id=None):
        if result:
            if restart:
                self.echo_response(message="VM %s successfully restarted" % vm_id)
            else:
                self.echo_response(message="VM %s successfully started" % vm_id)
        else:
            if restart:
                self.echo_response(message="VM %s failed to restart" % vm_id, err=True)
            else:
                self.echo_response(message="VM %s failed to start" % vm_id, err=True)

    def format_pause_output(self, result, vm_id):
        if result:
            self.echo_response(message="VM %s paused" % vm_id)
        else:
            self.echo_response(message="VM %s failed to pause" % vm_id, err=True)

    def format_shutdown_output(self, result, vm_id):
        if result:
            self.echo_response(message="VM %s is shutting down" % vm_id)
        else:
            self.echo_response(message="VM %s was unable to shut down (you can try with --force)" % vm_id, err=True)

    def format_reboot_output(self, result, vm_id):
        if result:
            self.echo_response(message="VM %s is rebooting" % vm_id)
        else:
            self.echo_response(message="VM %s was unable to reboot (you can try with --force)" % vm_id, err=True)

    def format_delete_output(self, result, vm_id):
        if result:
            self.echo_response(message="VM %s deleted successfully" % vm_id)
        else:
            self.echo_response(message="Unable to delete VM %s " % vm_id, err=True)

    def format_vm_not_exist(self):
        self.echo_response(message="vm does not exist", err=True)

    def echo_status_ok(self, message=''):
        self.echo_response(message=message)

    def echo_status_failure(self, message=''):
        self.echo_response(err=True, message=message)

    def format_properties_changed(self, succeeded, failed):
        self.echo_response(body={'succeeded': succeeded, 'failed': failed})

    def format_added_port_forwarding_rule(self, result):
        if result:
            self.echo_status_ok(message='rule added successfully')
        else:
            self.echo_status_failure(message='could not add port forwarding')

    def format_deleted_port_forwarding_rule(self, result):
        if result:
            self.echo_status_ok(message='rule deleted successfully')
        else:
            self.echo_status_failure(message='could not delete port forwarding')

    def format_describe(self, vm_dict):
        self.echo_response(body=vm_dict)

    def format_create(self, success):
        if success:
            self.echo_response(body={'uuid': success}, message="vm created successfully")
        else:
            self.echo_status_failure("could not create vm")

    def format_add_network_card(self, success):
        if success:
            self.echo_status_ok('successfully added network card')
        else:
            self.echo_status_failure('could not add network card')

    def format_delete_network_card(self, success):
        if success:
            self.echo_status_ok('successfully deleted network card')
        else:
            self.echo_status_failure('could not delete network card')