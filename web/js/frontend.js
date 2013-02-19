/**
 * fnordlicht web control frontend
 *
 * simple ajax colorwheel frontend to control fnordlicht's
 *
 * @copyright	2013 Steffen Vogel
 * @license	http://www.gnu.org/licenses/gpl.txt GNU Public License
 * @author	Steffen Vogel <post@steffenvogel.de>
 * @link	http://www.steffenvogel.de
 */
/*
 * This file is part of libfn
 *
 * libfn is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * libfn is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with libfn. If not, see <http://www.gnu.org/licenses/>.
 */

var wheel; /* the colorwheel object */
var state = { };
var fade = { step : { } };
var seed = Math.random();
var listener;

Math.sgn = function(x) {
	return (x == 0) ? 0 : (x < 0) ? -1 : 1;
}

function computeFade(current, target, step) {
	/* search for max distance */
	var max = 'r';
	var dist = 0;

	for (var i in target) {
		if (['r', 'g', 'b'].indexOf(i) == -1) continue;

		var d = Math.abs(target[i] - current[i]);
		if (d > dist) {
			max = i;
			dist = d;
		}
	}

	/* adjust fading speeds, relative to max distance */
	fade.step[max] = Math.sgn(target[max] - current[max]) * step;
	fade.steps = dist / step;

	for (var i in current) {
		if (['r', 'g', 'b'].indexOf(i) == -1 || i == max) continue;

		if (dist > 0) {
			var d = target[i] - current[i];
			var ratio = d / dist;

			fade.step[i] = ratio * step;
		}
		else {
			fade.step[i] = 0;
		}
	}
}

function wheelFade(current, target, step, delay) {
	if (current == target) return;

	window.clearInterval(fade.interval);
	computeFade(current, target, step, delay);
	fade.interval = window.setInterval(function() {
		fade.steps--;

		current.r += fade.step.r;
		current.g += fade.step.g;
		current.b += fade.step.b;
		current.hex = Raphael.rgb(current.r, current.g, current.b);

		if (fade.steps <= 0) {
			current = target;
			window.clearInterval(fade.interval);
		}

		state.color = current;
		setColor(current);
	}, delay * 10);
}

function getUrlParams() {
	var vars = [], hash;
	var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');

	for(var i = 0; i < hashes.length; i++) {
		hash = hashes[i].split('=');
		vars.push(hash[0]);
		vars[hash[0]] = hash[1];
	}

	return vars;
}

function fnStop() {
	$.ajax({
		type: 'POST',
		url: 'stop',
		async: false
	});
}

function fnStart(script) {
	var data = {
		script: script,
		step: state.step,
		delay: state.delay,
		sleep: state.sleep,
		value: state.value,
		saturation: state.saturation,
		use_address: (state.use_address) ? 1 : 0,
		wait_for_fade: (state.wait_for_fade) ? 1 : 0
	};

	$.ajax({
		type: 'POST',
		url: 'start?' + $.param(data)
	});
}

function fnFade(color) {
	if (fade.running) return;
	fade.running = true;

	var data = {
		color: color.hex,
		step: (fade.drag) ? 255 : state.step,
		delay: (fade.drag) ? 0 : state.delay
	};

	if (state.mask.indexOf('0') >= 0) {
		data.mask = state.mask;
	}

	if ($('#stop_fade').attr('checked')) {
		fnStop();
	}

	$.ajax({
		type: 'POST',
		url: 'fade?' + $.param(data),
		success: function() {
			fade.running = false;
		}
	});
}

function setColor(color) {
	$(document.body).css('background-color', color.hex);
	wheel.color(color.hex);
}

function listenCallback(data) {
	wheelFade(state.color, data.color, data.step, data.delay);

	state.count = data.count;
	state.users = data.users;

	drawUsers(data.users);

	/* restart listener */
	listener = $.get('status', { comet: 10 }, listenCallback);
}

function drawLamps(count) {
	state.mask = new String;

	$('#mask').empty();
	for (var i = 0; i < count; i++) {
		$('#mask').append(
			$('<img>')
				.addClass('icon').css('cursor', 'pointer')
				.attr('src', 'img/bulb-on.png').attr('alt', i)
				.data('index', i)
				.toggle(function() {
					$(this).attr('src', 'img/bulb-off.png');
					var idx = $(this).data('index');
					state.mask = state.mask.substr(0, idx) + '0' + state.mask.substr(idx+1); 
				}, function() {
					$(this).attr('src', 'img/bulb-on.png');
					var idx = $(this).data('index');
					state.mask = state.mask.substr(0, i) + '1' + state.mask.substr(i+1); 
				})
		);

		state.mask += '1';
	}
}

function drawUsers(count) {
	$('#users').empty();
	for (var i = 0; i <= count; i++) {
		$('#users').append(
			$('<img>')
				.addClass('icon').attr('alt', i)
				.attr('src', 'img/lego' + (Math.ceil(seed * 100 + i) % 6 + 1) + '.png')
		);
	}
}

$(document).ready(function() {
	/* initialize wheel */
	wheel = Raphael.colorwheel($('#wheel')[0], 500, 200);
	wheel.ondrag(function() {
		fade.dragTimeout = window.setTimeout(function() {
			fade.drag = true;
			listener.abort();
		}, 200);
	}, function(color) {
		if (fade.drag) {
			state.color = color;
			listener = $.get('status', { comet: 10 }, listenCallback);
		}
		else
			fnFade(color);

		fade.drag = false;
		window.clearTimeout(fade.dragTimeout);
	});


	wheel.onchange(function(color) {
		if (fade.drag) {
			$(document.body).css('background-color', color.hex);
			fnFade(color);
		}
	});

	/* show details */						
	$('#show_details').click(function() {
		$('#details').toggle();
		$('#show_details').toggle();
	});

	$('#options input')
	.change(function() {
		var elm = $(this);

		var key = elm.attr('id');
		var val = elm.val();

		switch (elm.attr('type')) {
			case 'text':
				val = parseInt(val);
				break;
			case 'checkbox':
				val = (elm.attr('checked') == 'checked');
				break;
		}

		state[key] = val;
	})
	.each(function(idx, elm) {
		$(elm).change();
	});

	/* start listener for first time*/
	listener = $.get('status', function(data) {
		state.color = data.color;
		setColor(state.color);

		/* draw lamps */
		drawLamps(data.count);

		/* parse url parameters */
		window.setTimeout(function() {
			var params = getUrlParams();
			if ('party' in params) {
				fnStart(('script' in params) ? parseInt(params['script']) : 1);
			}
			else if ('fade' in params) {
				if ('step' in params) state.step = parseInt(params['step']);
				if ('delay' in params) state.delay = parseInt(params['delay']);

				fnFade(Raphael.getRGB(params['fade']));
			}
		}, 100);

		listenCallback(data);
	});
});
