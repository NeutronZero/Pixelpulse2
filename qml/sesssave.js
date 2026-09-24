var hasOwn = function(object, key) {
    return Object.prototype.hasOwnProperty.call(object, key);
};

var finiteNumber = function(value) {
    var number = Number(value);
    return isFinite(number) ? number : null;
};

var clampedSlider = function(value, minimum, maximum) {
    var number = Number(value);
    if (!isFinite(number))
        return null;
    return Math.min(maximum, Math.max(minimum, number));
};

var supportedSampleTime = function(value) {
    var time = finiteNumber(value);
    if (time === null)
        return null;
    var supported = [0.01, 0.1, 1, 10];
    for (var i = 0; i < supported.length; i++) {
        if (Math.abs(time - supported[i]) < 0.000001)
            return supported[i];
    }
    return null;
};

var setPlotsVisible = function(value) {
    toolbar.plotsVisible = !!value;
    toolbar.colorDialog.plotCheckBox.checked = !!value;
};

var stateKey = function(deviceIndex, channel, signal) {
    return '' + deviceIndex + (channel ? channel.label : "") + "_"
        + (signal ? signal.label : "");
};

var saveState = function() {
    var signalStates = { schemaVersion: 1 };
    if (typeof deviceRepeater === "undefined" || !deviceRepeater) {
        return signalStates;
    }

    for (var a = 0; a < deviceRepeater.count; a++) {
        var deviceItem = deviceRepeater.itemAt(a);
        if (!deviceItem || !deviceItem.channelRepeater) {
            continue;
        }
        for (var b = 0; b < deviceItem.channelRepeater.count; b++) {
            var channelItem = deviceItem.channelRepeater.itemAt(b);
            var channel = channelItem ? channelItem.channel : null;
            if (!channel || !channelItem.signalRepeater) {
                continue;
            }
            for (var c = 0; c < channelItem.signalRepeater.count; c++) {
                var signalItem = channelItem.signalRepeater.itemAt(c);
                var signal = signalItem ? signalItem.signal : null;
                if (!signal) {
                    continue;
                }
                signalStates[stateKey(a, channel, signal)] = {
                    src: signal.src.src,
                    v1: signal.src.v1,
                    v2: signal.src.v2,
                    period: signal.src.period,
                    phase: signal.src.phase,
                    duty: signal.src.duty,
                    xscale: signalItem.xaxis ? signalItem.xaxis.xscale : 1,
                    ymin: signalItem.ymin,
                    ymax: signalItem.ymax,
                    mode: channel.mode
                };
            }
        }
    }

    signalStates.generalSettings = {
        signalCheckBox: toolbar.colorDialog.sigCheckBox.checked,
        plotsCheckBox: toolbar.colorDialog.plotCheckBox.checked,
        sliderBrightness: toolbar.colorDialog.sliderB.value,
        sliderContrast: toolbar.colorDialog.sliderC.value,
        sliderPhosphor: toolbar.colorDialog.sliderPh.value,
        sliderDotSize: toolbar.colorDialog.sliderDot.value,
        xy_checked: plotsVisible,
        repeatedSweep: repeatedSweep,
        sampleTime: controller.sampleTime
    };
    return signalStates;
};

var restoreState = function(signalStates) {
    if (!signalStates || typeof signalStates !== "object") {
        return false;
    }
    if (typeof deviceRepeater === "undefined" || !deviceRepeater) {
        return false;
    }

    var updates = [];
    for (var a = 0; a < deviceRepeater.count; a++) {
        var deviceItem = deviceRepeater.itemAt(a);
        if (!deviceItem || !deviceItem.channelRepeater) {
            continue;
        }
        for (var b = 0; b < deviceItem.channelRepeater.count; b++) {
            var channelItem = deviceItem.channelRepeater.itemAt(b);
            var channel = channelItem ? channelItem.channel : null;
            if (!channel || !channelItem.signalRepeater) {
                continue;
            }
            for (var c = 0; c < channelItem.signalRepeater.count; c++) {
                var signalItem = channelItem.signalRepeater.itemAt(c);
                var signal = signalItem ? signalItem.signal : null;
                if (!signal) {
                    continue;
                }
                var state = signalStates[stateKey(a, channel, signal)];
                if (!state || typeof state !== "object") {
                    continue;
                }

                var mode = finiteNumber(state.mode);
                var v1 = finiteNumber(state.v1);
                var v2 = finiteNumber(state.v2);
                var period = finiteNumber(state.period);
                var phase = finiteNumber(state.phase);
                var duty = finiteNumber(state.duty);
                var ymin = finiteNumber(state.ymin);
                var ymax = finiteNumber(state.ymax);
                var xscale = finiteNumber(state.xscale);
                var sources = {constant: true, square: true, sawtooth: true,
                               stairstep: true, sine: true, triangle: true};
                if (mode === null || mode < 0 || mode > 2 || Math.floor(mode) !== mode
                        || !hasOwn(sources, state.src) || v1 === null || v2 === null
                        || period === null || (state.src !== "constant" && period <= 0)
                        || phase === null
                        || duty === null || duty < 0 || duty > 1
                        || ymin === null || ymax === null || ymin >= ymax
                        || xscale === null || xscale <= 0) {
                    continue;
                }
                updates.push({
                    signalItem: signalItem,
                    signal: signal,
                    channel: channel,
                    channelRow: signalItem.channelRow,
                    state: state,
                    mode: mode,
                    xscale: xscale
                });
            }
        }
    }

    for (var i = 0; i < updates.length; i++) {
        var update = updates[i];
        update.channel.mode = update.mode;
        if (update.channelRow && update.channelRow.applyMode)
            update.channelRow.applyMode();
        update.signal.src.src = update.state.src;
        update.signal.src.v1 = update.state.v1;
        update.signal.src.v2 = update.state.v2;
        update.signal.src.period = update.state.period;
        update.signal.src.phase = update.state.phase;
        update.signal.src.duty = update.state.duty;
        update.signalItem.ymin = update.state.ymin;
        update.signalItem.ymax = update.state.ymax;
        if (update.signalItem.xaxis) {
            update.signalItem.xaxis.xscale = update.xscale;
        }
    }

    var general = signalStates.generalSettings;
    if (!general || typeof general !== "object") {
        return updates.length > 0;
    }
    if (hasOwn(general, "signalCheckBox")) {
        toolbar.colorDialog.sigCheckBox.checked = !!general.signalCheckBox;
    }
    if (hasOwn(general, "plotsCheckBox")) {
        setPlotsVisible(general.plotsCheckBox);
    }
    if (hasOwn(general, "sliderBrightness")) {
        var brightness = clampedSlider(general.sliderBrightness, 0, 1);
        if (brightness !== null)
            toolbar.colorDialog.sliderB.value = brightness;
    }
    if (hasOwn(general, "sliderContrast")) {
        var contrast = clampedSlider(general.sliderContrast, 0, 1);
        if (contrast !== null)
            toolbar.colorDialog.sliderC.value = contrast;
    }
    if (hasOwn(general, "sliderPhosphor")) {
        var phosphor = clampedSlider(general.sliderPhosphor, 0, 1);
        if (phosphor !== null)
            toolbar.colorDialog.sliderPh.value = phosphor;
    }
    if (hasOwn(general, "sliderDotSize")) {
        var dotSize = clampedSlider(general.sliderDotSize, 0.1, 1);
        if (dotSize !== null)
            toolbar.colorDialog.sliderDot.value = dotSize;
    }
    if (hasOwn(general, "xy_checked")) {
        setPlotsVisible(general.xy_checked);
    }
    if (hasOwn(general, "repeatedSweep")) {
        repeatedSweep = !!general.repeatedSweep;
    }
    if (hasOwn(general, "sampleTime")) {
        var sampleTime = supportedSampleTime(general.sampleTime);
        if (sampleTime !== null) {
            controller.sampleTime = sampleTime;
        }
    }
    return updates.length > 0;
};
