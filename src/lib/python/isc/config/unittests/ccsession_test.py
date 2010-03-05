# Copyright (C) 2010  Internet Systems Consortium.
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SYSTEMS CONSORTIUM
# DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
# INTERNET SYSTEMS CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
# INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
# FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
# NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
# WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

#
# Tests for the ConfigData and MultiConfigData classes
#

import unittest
import os
from isc.config.ccsession import *
from unittest_fakesession import FakeModuleCCSession

class TestHelperFunctions(unittest.TestCase):
    def test_parse_answer(self):
        self.assertRaises(ModuleCCSessionError, parse_answer, 1)
        self.assertRaises(ModuleCCSessionError, parse_answer, { 'just a dict': 1 })
        self.assertRaises(ModuleCCSessionError, parse_answer, { 'result': 1 })
        self.assertRaises(ModuleCCSessionError, parse_answer, { 'result': [] })
        self.assertRaises(ModuleCCSessionError, parse_answer, { 'result': [ 'not_an_rcode' ] })
        self.assertRaises(ModuleCCSessionError, parse_answer, { 'result': [ 1, 2 ] })
        
        rcode, val = parse_answer({ 'result': [ 0 ] })
        self.assertEqual(0, rcode)
        self.assertEqual(None, val)
        rcode, val = parse_answer({ 'result': [ 0, "something" ] })
        self.assertEqual(0, rcode)
        self.assertEqual("something", val)
        rcode, val = parse_answer({ 'result': [ 1, "some error" ] })
        self.assertEqual(1, rcode)
        self.assertEqual("some error", val)

    def test_create_answer(self):
        self.assertRaises(ModuleCCSessionError, create_answer, 'not_an_int')
        self.assertRaises(ModuleCCSessionError, create_answer, 1, 2)
        self.assertRaises(ModuleCCSessionError, create_answer, 1)
        self.assertEqual({ 'result': [ 0 ] }, create_answer(0))
        self.assertEqual({ 'result': [ 1, 'something bad' ] }, create_answer(1, 'something bad'))
        self.assertEqual({ 'result': [ 0, 'something good' ] }, create_answer(0, 'something good'))
        self.assertEqual({ 'result': [ 0, ['some', 'list' ] ] }, create_answer(0, ['some', 'list']))
        self.assertEqual({ 'result': [ 0, {'some': 'map' } ] }, create_answer(0, {'some': 'map'}))

    def test_parse_command(self):
        cmd, arg = parse_command(1)
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({})
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'not a command': 1})
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'command': 1})
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'command': []})
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'command': [ 1 ]})
        self.assertEqual(None, cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'command': [ 'command' ]})
        self.assertEqual('command', cmd)
        self.assertEqual(None, arg)

        cmd, arg = parse_command({ 'command': [ 'command', 1 ]})
        self.assertEqual('command', cmd)
        self.assertEqual(1, arg)

        cmd, arg = parse_command({ 'command': [ 'command', ['some', 'argument', 'list'] ]})
        self.assertEqual('command', cmd)
        self.assertEqual(['some', 'argument', 'list'], arg)

    def test_create_command(self):
        self.assertRaises(ModuleCCSessionError, create_command, 1)
        self.assertEqual({'command': [ 'my_command' ]}, create_command('my_command'))
        self.assertEqual({'command': [ 'my_command', 1 ]}, create_command('my_command', 1))
        self.assertEqual({'command': [ 'my_command', [ 'some', 'list' ] ]}, create_command('my_command', [ 'some', 'list' ]))
        self.assertEqual({'command': [ 'my_command', { 'some': 'map' } ]}, create_command('my_command', { 'some': 'map' }))

class TestModuleCCSession(unittest.TestCase):
    def setUp(self):
        if 'CONFIG_TESTDATA_PATH' in os.environ:
            self.data_path = os.environ['CONFIG_TESTDATA_PATH']
        else:
            self.data_path = "../../../testdata"

    def spec_file(self, file):
        return self.data_path + os.sep + file
        
    def create_session(self, spec_file_name, config_handler = None, command_handler = None, cc_session = None):
        return ModuleCCSession(self.spec_file(spec_file_name), config_handler, command_handler, cc_session)

    def test_init(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        self.assertEqual(isc.config.module_spec_from_file(self.spec_file("spec1.spec"))._module_spec, mccs.specification._module_spec)
        self.assertEqual(None, mccs._config_handler)
        self.assertEqual(None, mccs._command_handler)

    def test_start1(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        self.assertEqual(len(fake_session.message_queue), 0)
        self.assertRaises(ModuleCCSessionError, mccs.start)
        self.assertEqual(len(fake_session.message_queue), 2)
        self.assertEqual({'command': ['module_spec', {'module_name': 'Spec1'}]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec1'}]},
                         fake_session.get_message('ConfigManager', None))

        self.assertEqual(len(fake_session.message_queue), 0)
        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec1")
        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec1")
        mccs.start()
        self.assertEqual(len(fake_session.message_queue), 2)

        self.assertEqual({'command': ['module_spec', {'module_name': 'Spec1'}]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec1'}]},
                         fake_session.get_message('ConfigManager', None))

    def test_start2(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        self.assertEqual(len(fake_session.message_queue), 0)
        self.assertRaises(ModuleCCSessionError, mccs.start)
        self.assertEqual(len(fake_session.message_queue), 2)
        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

        self.assertEqual(len(fake_session.message_queue), 0)
        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec2")
        fake_session.group_sendmsg({'result': [ 0, {} ]}, "Spec2")
        mccs.start()
        self.assertEqual(len(fake_session.message_queue), 2)

        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

    def test_start3(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_ok)
        self.assertEqual(len(fake_session.message_queue), 0)
        self.assertRaises(ModuleCCSessionError, mccs.start)
        self.assertEqual(len(fake_session.message_queue), 2)
        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

        self.assertEqual(len(fake_session.message_queue), 0)
        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec2")
        fake_session.group_sendmsg({'result': [ 0, {} ]}, "Spec2")
        mccs.start()
        self.assertEqual(len(fake_session.message_queue), 2)

        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

    def test_start4(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_ok)
        self.assertEqual(len(fake_session.message_queue), 0)
        self.assertRaises(ModuleCCSessionError, mccs.start)
        self.assertEqual(len(fake_session.message_queue), 2)
        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

        self.assertEqual(len(fake_session.message_queue), 0)
        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec2")
        fake_session.group_sendmsg({'result': [ 1, "just an error" ]}, "Spec2")
        mccs.start()
        self.assertEqual(len(fake_session.message_queue), 2)

        self.assertEqual({'command': ['module_spec', mccs.specification._module_spec]},
                         fake_session.get_message('ConfigManager', None))
        self.assertEqual({'command': ['get_config', {'module_name': 'Spec2'}]},
                         fake_session.get_message('ConfigManager', None))

    def test_get_socket(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        self.assertNotEqual(None, mccs.get_socket())

    def test_get_session(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        self.assertEqual(fake_session, mccs.get_session())

    def test_close(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        mccs.close()
        self.assertEqual("closed", fake_session._socket)

    def my_config_handler_ok(self, new_config):
        return isc.config.ccsession.create_answer(0)

    def my_config_handler_err(self, new_config):
        return isc.config.ccsession.create_answer(1, "just an error")

    def my_config_handler_exc(self, new_config):
        raise Exception("just an exception")

    def my_command_handler_ok(self, command, args):
        return isc.config.ccsession.create_answer(0)

    def my_command_handler_no_answer(self, command, args):
        pass

    def test_check_command1(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)

        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 0)

        fake_session.group_sendmsg({'result': [ 0 ]}, "Spec1")
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 0)

        cmd = isc.config.ccsession.create_command(isc.config.ccsession.COMMAND_CONFIG_UPDATE, { 'Spec1': 'a' })

        fake_session.group_sendmsg(cmd, 'Spec1')
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [2, 'Spec1 has no config handler']},
                         fake_session.get_message('Spec1', None))

    def test_check_command2(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec1.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_ok)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command(isc.config.ccsession.COMMAND_CONFIG_UPDATE, { 'Spec1': 'a' })
        fake_session.group_sendmsg(cmd, 'Spec1')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [1, 'No config_data specification']},
                         fake_session.get_message('Spec1', None))
        
    def test_check_command3(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_ok)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command(isc.config.ccsession.COMMAND_CONFIG_UPDATE, { 'Spec2': { 'item1': 2 }})
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [0]},
                         fake_session.get_message('Spec2', None))
        
    def test_check_command4(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_err)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command(isc.config.ccsession.COMMAND_CONFIG_UPDATE, { 'Spec2': { 'item1': 'aaa' }})
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [1, 'just an error']},
                         fake_session.get_message('Spec2', None))
        
    def test_check_command5(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_config_handler(self.my_config_handler_exc)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command(isc.config.ccsession.COMMAND_CONFIG_UPDATE, { 'Spec2': { 'item1': 'aaa' }})
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [1, 'just an exception']},
                         fake_session.get_message('Spec2', None))
        
    def test_check_command6(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command("print_message", "just a message")
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [2, 'Spec2 has no command handler']},
                         fake_session.get_message('Spec2', None))
        
    def test_check_command7(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_command_handler(self.my_command_handler_ok)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command("print_message", "just a message")
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 1)
        self.assertEqual({'result': [0]},
                         fake_session.get_message('Spec2', None))
        
    def test_check_command8(self):
        fake_session = FakeModuleCCSession()
        mccs = self.create_session("spec2.spec", None, None, fake_session)
        mccs.set_command_handler(self.my_command_handler_no_answer)
        self.assertEqual(len(fake_session.message_queue), 0)
        cmd = isc.config.ccsession.create_command("print_message", "just a message")
        fake_session.group_sendmsg(cmd, 'Spec2')
        self.assertEqual(len(fake_session.message_queue), 1)
        mccs.check_command()
        self.assertEqual(len(fake_session.message_queue), 0)

class fakeUIConn():
    def __init__(self):
        self.get_answers = {}
        self.post_answers = {}

    def set_get_answer(self, name, answer):
        self.get_answers[name] = answer
    
    def set_post_answer(self, name, answer):
        self.post_answers[name] = answer
    
    def send_GET(self, name, arg = None):
        if name in self.get_answers:
            return self.get_answers[name]
        else:
            return None
    
    def send_POST(self, name, arg = None):
        if name in self.post_answers:
            return self.post_answers[name]
        else:
            return None
    

class TestUIModuleCCSession(unittest.TestCase):
    def setUp(self):
        if 'CONFIG_TESTDATA_PATH' in os.environ:
            self.data_path = os.environ['CONFIG_TESTDATA_PATH']
        else:
            self.data_path = "../../../testdata"

    def spec_file(self, file):
        return self.data_path + os.sep + file
        
    def create_uccs2(self, fake_conn):
        module_spec = isc.config.module_spec_from_file(self.spec_file("spec2.spec"))
        fake_conn.set_get_answer('/config_spec', { module_spec.get_module_name(): module_spec.get_config_spec()})
        fake_conn.set_get_answer('/commands', { module_spec.get_module_name(): module_spec.get_commands_spec()})
        fake_conn.set_get_answer('/config_data', { 'version': 1 })
        return UIModuleCCSession(fake_conn)

    def test_init(self):
        fake_conn = fakeUIConn()
        fake_conn.set_get_answer('/config_spec', {})
        fake_conn.set_get_answer('/commands', {})
        fake_conn.set_get_answer('/config_data', { 'version': 1 })
        uccs = UIModuleCCSession(fake_conn)
        self.assertEqual({}, uccs._specifications)
        self.assertEqual({ 'version': 1}, uccs._current_config)

        module_spec = isc.config.module_spec_from_file(self.spec_file("spec2.spec"))
        fake_conn.set_get_answer('/config_spec', { module_spec.get_module_name(): module_spec.get_config_spec()})
        fake_conn.set_get_answer('/commands', { module_spec.get_module_name(): module_spec.get_commands_spec()})
        fake_conn.set_get_answer('/config_data', { 'version': 1 })
        uccs = UIModuleCCSession(fake_conn)
        self.assertEqual(module_spec._module_spec, uccs._specifications['Spec2']._module_spec)

        fake_conn.set_get_answer('/config_data', { 'version': 123123 })
        self.assertRaises(ModuleCCSessionError, UIModuleCCSession, fake_conn)

    def test_add_remove_value(self):
        fake_conn = fakeUIConn()
        uccs = self.create_uccs2(fake_conn)
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.add_value, 1, "a")
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.add_value, "no_such_item", "a")
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.add_value, "Spec2/item1", "a")
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.remove_value, 1, "a")
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.remove_value, "no_such_item", "a")
        self.assertRaises(isc.cc.data.DataNotFoundError, uccs.remove_value, "Spec2/item1", "a")
        self.assertEqual({}, uccs._local_changes)
        uccs.add_value("Spec2/item5", "foo")
        self.assertEqual({'Spec2': {'item5': ['a', 'b', 'foo']}}, uccs._local_changes)
        uccs.remove_value("Spec2/item5", "foo")
        self.assertEqual({'Spec2': {'item5': ['a', 'b']}}, uccs._local_changes)
        uccs._local_changes = {'Spec2': {'item5': []}}
        uccs.remove_value("Spec2/item5", "foo")
        uccs.add_value("Spec2/item5", "foo")
        self.assertEqual({'Spec2': {'item5': ['foo']}}, uccs._local_changes)
        uccs.add_value("Spec2/item5", "foo")
        self.assertEqual({'Spec2': {'item5': ['foo']}}, uccs._local_changes)

    def test_commit(self):
        fake_conn = fakeUIConn()
        uccs = self.create_uccs2(fake_conn)
        uccs.commit()
        uccs._local_changes = {'Spec2': {'item5': [ 'a' ]}}
        uccs.commit()

if __name__ == '__main__':
    unittest.main()
