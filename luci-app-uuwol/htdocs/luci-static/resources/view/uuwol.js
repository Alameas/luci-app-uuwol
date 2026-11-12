// SPDX-License-Identifier: GPL-3.0-only

'use strict';
'require form';
'require uci';
'require view';

return view.extend({
	load: function() {
		return uci.load('uuwol');
	},

	render: function() {
		var m = new form.Map('uuwol', 'UU WOL');
		var s = m.section(form.NamedSection, 'config', 'uuwol');

		s.anonymous = false;
		s.addremove = false;

		var o = s.option(form.Flag, 'enabled', 'Enable');
		o.default = o.disabled;
		o.rmempty = false;

		return m.render();
	}
});
