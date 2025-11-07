%% Compare original 44.1 kHz and compensated 48 kHz filters
clear; close all; clc;

%----------------------------------------------
% 1. Sample rates
%----------------------------------------------
fs_orig = 44100;
fs_new  = 48000;

%----------------------------------------------
% 2. Filter 1: Pre-emphasis (H(z) = 1 - z^-1)
%----------------------------------------------
b_pre = [1, -1];
a_pre = [1];

[h_pre_orig, w_pre_orig] = freqz(b_pre, a_pre, 2048, fs_orig);
[h_pre_new, w_pre_new]   = freqz(b_pre, a_pre, 2048, fs_new);

figure(1);
subplot(2,1,1);
semilogx(w_pre_orig, 20*log10(abs(h_pre_orig)), 'b', 'LineWidth', 2);
hold on;
semilogx(w_pre_new, 20*log10(abs(h_pre_new)), 'r--', 'LineWidth', 1.5);
title('Filter 1: Pre-Emphasis');
xlabel('Frequency (Hz)'); ylabel('Magnitude (dB)');
legend('44.1 kHz','48 kHz'); grid on; axis tight;

subplot(2,1,2);
semilogx(w_pre_orig, unwrap(angle(h_pre_orig))*180/pi, 'b', 'LineWidth', 2);
hold on;
semilogx(w_pre_new, unwrap(angle(h_pre_new))*180/pi, 'r--', 'LineWidth', 1.5);
xlabel('Frequency (Hz)'); ylabel('Phase (°)');
grid on; axis tight;

%----------------------------------------------
% 3. Filter 2: Two all-pass (“smear”) sections
%----------------------------------------------
% --- Original 44.1 kHz coefficients
h0_orig = 0.3;
h1_orig = 0.77;

%----------------------------------------------
% Frequency-mapped coefficients for 48 kHz
%----------------------------------------------
% Preserve analog corner frequencies (prewarped mapping)
warp_func   = @(h) tan(acos(-h)/2);   % digital pole → analog freq
unwarp_func = @(w) -cos(2*atan(w));   % analog freq → digital pole

% Convert digital pole → analog freq
w0_analog = warp_func(h0_orig);
w1_analog = warp_func(h1_orig);

% Scale for new sample rate
scale = fs_new / fs_orig;

% Convert back analog → digital pole
h0_new = unwarp_func(w0_analog * scale);
h1_new = unwarp_func(w1_analog * scale);

%----------------------------------------------
% 4. Build cascades
%----------------------------------------------
% --- Original filter at 44.1 kHz
b1_orig = [h0_orig, 1];
a1_orig = [1, h0_orig];
b2_orig = [h1_orig, 1];
a2_orig = [1, h1_orig];
b2_delayed_orig = [0, b2_orig]; % delay Hq path

num1 = conv(b1_orig, a2_orig);
num2 = conv(b2_delayed_orig, a1_orig);
len = max(length(num1), length(num2));
b_total_orig = [num1, zeros(1, len - length(num1))] + ...
               [num2, zeros(1, len - length(num2))];
a_total_orig = conv(a1_orig, a2_orig);
[h_total_orig, w_total_orig] = freqz(b_total_orig, a_total_orig, 2048, fs_orig);

% --- New compensated filter at 48 kHz
b1_new = [h0_new, 1];
a1_new = [1, h0_new];
b2_new = [h1_new, 1];
a2_new = [1, h1_new];
b2_delayed_new = [0, b2_new];

num1 = conv(b1_new, a2_new);
num2 = conv(b2_delayed_new, a1_new);
len = max(length(num1), length(num2));
b_total_new = [num1, zeros(1, len - length(num1))] + ...
              [num2, zeros(1, len - length(num2))];
a_total_new = conv(a1_new, a2_new);
[h_total_new, w_total_new] = freqz(b_total_new, a_total_new, 2048, fs_new);

%----------------------------------------------
% 5. Plot comparison
%----------------------------------------------
figure(2);
subplot(2,1,1);
semilogx(w_total_orig, 20*log10(abs(h_total_orig)), 'b', 'LineWidth', 2);
hold on;
semilogx(w_total_new, 20*log10(abs(h_total_new)), 'r--', 'LineWidth', 1.5);
title('Filter 2: Two-Allpass “Smear”');
xlabel('Frequency (Hz)'); ylabel('Magnitude (dB)');
legend('44.1 kHz','48 kHz compensated');
grid on; axis tight;

subplot(2,1,2);
semilogx(w_total_orig, unwrap(angle(h_total_orig))*180/pi, 'b', 'LineWidth', 2);
hold on;
semilogx(w_total_new, unwrap(angle(h_total_new))*180/pi, 'r--', 'LineWidth', 1.5);
xlabel('Frequency (Hz)'); ylabel('Phase (°)');
grid on; axis tight;

fprintf('\nComputed 48 kHz compensated coefficients:\n');
fprintf('  h0 = %.6f\n', h0_new);
fprintf('  h1 = %.6f\n', h1_new);
