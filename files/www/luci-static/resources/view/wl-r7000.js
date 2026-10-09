'use strict';
'require view';
'require form';
'require fs';
'require poll';

// Radios BCM4360 do R7000 no driver wl da Broadcom: fora do cfg80211, por
// isso nao aparecem em Rede > Wireless. Config em /etc/config/wl-r7000;
// salvar dispara o reload do servico wl-r7000 (refaz os radios).

var RADIOS = [
	{ id: 'wl0', title: '2.4 GHz', chans: [ '1', '6', '11', '1/40', '6/40' ] },
	{ id: 'wl1', title: '5 GHz', chans: [ '36/80', '149/80', '36/40', '149/40', '36', '149' ] }
];

function wl(ifc, args) {
	return fs.exec('/usr/sbin/wl', [ '-i', ifc ].concat(args))
		.then(function(r) { return (r.code == 0) ? (r.stdout || '') : ''; })
		.catch(function() { return ''; });
}

function status(ifc) {
	return Promise.all([
		wl(ifc, [ 'status' ]),
		wl(ifc, [ 'assoclist' ])
	]).then(function(r) {
		var macs = r[1].split('\n').map(function(l) {
			var m = l.match(/assoclist\s+([0-9A-Fa-f:]{17})/);
			return m ? m[1] : null;
		}).filter(Boolean);

		return Promise.all(macs.map(function(mac) {
			return wl(ifc, [ 'rssi', mac ]).then(function(s) {
				return [ mac, s.trim() ];
			});
		})).then(function(cli) {
			var st = r[0], g = function(re) { var m = st.match(re); return m ? m[1] : '?'; };
			return {
				ssid: g(/SSID: "([^"]*)"/),
				chan: g(/Chanspec: ([^(\n]*?)\s*\(/) != '?' ? g(/Chanspec: ([^(\n]*?)\s*\(/) : g(/Channel: (\S+)/),
				noise: g(/noise: (-?\d+)/),
				clients: cli
			};
		});
	});
}

function render_status(node, ifc) {
	return status(ifc).then(function(s) {
		var rows = s.clients.map(function(c) {
			return E('tr', { 'class': 'tr' }, [
				E('td', { 'class': 'td' }, c[0]),
				E('td', { 'class': 'td' }, c[1] + ' dBm')
			]);
		});
		if (!rows.length)
			rows = [ E('tr', { 'class': 'tr' }, E('td', { 'class': 'td', 'colspan': 2 }, E('em', 'nenhum cliente'))) ];

		node.replaceChildren(
			E('p', [ 'SSID ', E('strong', s.ssid), ' · canal ', s.chan, ' · ruido ', s.noise, ' dBm' ]),
			E('table', { 'class': 'table' }, [
				E('tr', { 'class': 'tr table-titles' }, [
					E('th', { 'class': 'th' }, 'Cliente'),
					E('th', { 'class': 'th' }, 'Sinal')
				])
			].concat(rows))
		);
	});
}

return view.extend({
	render: function() {
		var m = new form.Map('wl-r7000', 'Wifi R7000',
			'Radios BCM4360 no driver wl da Broadcom. Salvar e aplicar refaz os radios (os clientes caem por alguns segundos).');

		RADIOS.forEach(function(r) {
			var s = m.section(form.NamedSection, r.id, 'radio', r.title + ' (' + r.id + ')');
			var o;

			o = s.option(form.DummyValue, '_status', 'Estado');
			o.rawhtml = true;
			o.cfgvalue = function() {
				var node = E('div', { 'id': 'wl-status-' + r.id }, E('em', 'lendo...'));
				render_status(node, r.id);
				poll.add(function() { return render_status(node, r.id); }, 10);
				return node;
			};

			o = s.option(form.Flag, 'enabled', 'Ligado');
			o.rmempty = false;

			o = s.option(form.Value, 'ssid', 'SSID');
			o.datatype = 'maxlength(32)';

			o = s.option(form.Value, 'key', 'Senha WPA2');
			o.password = true;
			o.datatype = 'wpakey';

			o = s.option(form.Value, 'chanspec', 'Canal', 'canal ou canal/largura, ex.: 36/80');
			r.chans.forEach(function(c) { o.value(c); });

			o = s.option(form.Value, 'country', 'Pais/revisao do CLM', 'BR/2 libera 80 MHz no 5 GHz');
			o.placeholder = 'BR/2';

			o = s.option(form.Value, 'bridge', 'Bridge');
			o.placeholder = 'br-lan';
		});

		return m.render();
	}
});
