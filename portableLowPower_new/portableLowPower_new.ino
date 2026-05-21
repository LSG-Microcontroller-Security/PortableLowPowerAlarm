#include <Arduino.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/power.h>
#include <avr/sleep.h>
#include <avr/wdt.h>
#include <SoftwareSerial.h>
#include <string.h>

#define SIM_SERIAL_RX_PIN PB4
#define SIM_SERIAL_TX_PIN PB3
#define SIM_SLEEP_PIN PB0
#define SIM_SERIAL_BAUD_RATE 9600UL
#define SIM_SERIAL_INVERSE_LOGIC false

#define APP_DEBUG_SERIAL_ENABLED 1U

#define SIM_BOOT_DELAY_MS 3000U

#define SIM_SMS_CMD_SET_MEMORY "AT+CPMS=\"SM\",\"SM\",\"SM\""
#define SIM_SMS_CMD_TEXT_MODE "AT+CMGF=1"
#define SIM_SMS_CMD_CHARSET "AT+CSCS=\"GSM\""
#define SIM_SMS_CMD_CNMI "AT+CNMI=2,1,0,0,0"
#define SIM_SMS_CMD_GET_STORAGE_STATUS "AT+CPMS?"
#define SIM_SMS_CMD_READ_PREFIX "AT+CMGR="
#define SIM_SMS_CMD_DELETE_ALL "AT+CMGD=1,4"
#define SIM_SMS_CMD_ENABLE_DTR_SLEEP "AT+CSCLK=1"
#define SIM_SMS_CMD_BASIC_AT "AT"
#define SIM_SMS_CMD_AUTO_ANSWER_3_RINGS "ATS0=3"
#define SIM_SMS_CMD_AUTO_ANSWER_DISABLED "ATS0=0"

#define SIM_NETLIGHT_CMD_OFF "AT+CNETLIGHT=0"
#define SIM_NETLIGHT_TIMEOUT_MS 35000UL
#define SIM_NETLIGHT_PRE_DELAY_MS 5000UL
#define SIM_NETLIGHT_RETRY_DELAY_MS 400U
#define SIM_NETLIGHT_MAX_ATTEMPTS 3U

#define SIM_SMS_LITERAL_OK "OK"
#define SIM_SMS_LITERAL_ERROR "ERROR"
#define SIM_SMS_STORAGE_HEADER "+CPMS:"
#define SIM_SMS_HEADER "+CMGR:"
#define SIM_SMS_RESPONSE_OK_WITH_CRLF "\r\nOK\r\n"
#define SIM_SMS_RESPONSE_ERROR_WITH_CRLF "\r\nERROR\r\n"
#define SIM_SMS_RESPONSE_OK_SUFFIX "OK\r\n"
#define SIM_SMS_RESPONSE_ERROR_SUFFIX "ERROR\r\n"

#define SIM_SMS_SETUP_TIMEOUT_MS 5000UL
#define SIM_SMS_STORAGE_TIMEOUT_MS 3000UL
#define SIM_READ_SMS_TIMEOUT_MS 3000UL
#define SIM_SMS_SETUP_STEP_DELAY_MS 1000U
#define SIM_SLEEP_ENTER_PRE_DELAY_MS 5000U
#define SIM_SLEEP_EXIT_PRE_DELAY_MS 200U
#define SIM_SLEEP_CMD_TIMEOUT_MS 3000UL
#define SIM_CALL_START_TIMEOUT_MS 5000UL
#define SIM_CALL_POST_DIAL_DELAY_MS 1500U
#define SIM_POST_WAKE_SETTLE_MS 700U
#define SIM_SMS_RESPONSE_BUFFER_SIZE 32U
#define SIM_SMS_MESSAGE_BUFFER_SIZE 12U

#define WATCHDOG_TICK_SECONDS 8U
#define SMS_POLL_INTERVAL_SECONDS 60U
#define WATCHDOG_POLL_INTERVAL_TICKS_RAW ((uint8_t)(((unsigned long)SMS_POLL_INTERVAL_SECONDS + (WATCHDOG_TICK_SECONDS - 1U)) / WATCHDOG_TICK_SECONDS))
#define WATCHDOG_POLL_INTERVAL_TICKS ((WATCHDOG_POLL_INTERVAL_TICKS_RAW == 0U) ? 1U : WATCHDOG_POLL_INTERVAL_TICKS_RAW)
#define ACTIVE_POLL_DELAY_MS 1000U

#define SIM_INVALID_SMS_COUNT (-1)
#define SIM_SMS_SLOT_FIRST 1U
#define APP_PHONE_LOCAL_NUMBER_LENGTH 10U
#define EEPROM_PHONE_BASE_ADDRESS 0U
#define SMS_CMD_STORE_PHONE '#'
#define SMS_CMD_AUTO_ANSWER_ENABLE '1'
#define SMS_CMD_AUTO_ANSWER_DISABLE '2'
#define SIM_CHAR_END_OF_STRING '\0'
#define SIM_CHAR_CARRIAGE_RETURN ((uint8_t)'\r')
#define DEBUG_CHAR_LINE_FEED ((uint8_t)'\n')
#define APP_PHONE_COUNTRY_PREFIX_CHAR_1 ((uint8_t)'+')
#define APP_PHONE_COUNTRY_PREFIX_CHAR_2 ((uint8_t)'3')
#define APP_PHONE_COUNTRY_PREFIX_CHAR_3 ((uint8_t)'9')

SoftwareSerial sim_serial(SIM_SERIAL_RX_PIN, SIM_SERIAL_TX_PIN, SIM_SERIAL_INVERSE_LOGIC);
bool sms_receiver_initialized = false;
volatile bool watchdog_wake_state = false;
bool sim800c_sleep_state = false;
bool sleep_mode_enabled = true;
uint8_t watchdog_poll_elapsed_ticks = 0U;

#if APP_DEBUG_SERIAL_ENABLED
#define DEBUG_SERIAL_RX_PIN PB2
#define DEBUG_SERIAL_TX_PIN PB1
#define DEBUG_SERIAL_BAUD_RATE 9600UL
#define DEBUG_SERIAL_INVERSE_LOGIC false
SoftwareSerial debug_serial(DEBUG_SERIAL_RX_PIN, DEBUG_SERIAL_TX_PIN, DEBUG_SERIAL_INVERSE_LOGIC);

void debug_print_fail(const __FlashStringHelper* fail_code) {
	debug_serial.print(fail_code);
	debug_serial.write(SIM_CHAR_CARRIAGE_RETURN);
	debug_serial.write(DEBUG_CHAR_LINE_FEED);
}

#define DEBUG_FAIL(text_literal) debug_print_fail(F(text_literal))
#define DEBUG_LOG(text_literal) debug_print_fail(F(text_literal))
#else
#define DEBUG_FAIL(...) ((void)0)
#define DEBUG_LOG(...) ((void)0)
#endif

ISR(WDT_vect) {
	watchdog_wake_state = true;
}

void send_at_cmd(const char* cmd) {
	sim_serial.print(cmd);
	sim_serial.write(SIM_CHAR_CARRIAGE_RETURN);
}
bool wait_for_pattern(const char* pattern, unsigned long timeout_ms) {
	const char* match = pattern;
	unsigned long start = millis();

	while (static_cast<unsigned long>(millis() - start) < timeout_ms) {
		if (sim_serial.available() <= 0) {
			continue;
		}

		int read_value = sim_serial.read();
		if (read_value < 0) {
			continue;
		}

		char c = static_cast<char>(read_value);
		if (c == *match) {
			++match;
			if (*match == SIM_CHAR_END_OF_STRING) {
				return true;
			}
		}
		else {
			match = (c == pattern[0]) ? pattern + 1 : pattern;
			if (*match == SIM_CHAR_END_OF_STRING) {
				return true;
			}
		}
	}

	return false;
}
void configure_watchdog_8s_interrupt() {
#if defined(MCUSR)
	MCUSR &= static_cast<uint8_t>(~_BV(WDRF));
#elif defined(MCUCSR)
	MCUCSR &= static_cast<uint8_t>(~_BV(WDRF));
#endif

#if defined(WDCE)
	const uint8_t watchdog_change_enable = _BV(WDCE);
#elif defined(WDTOE)
	const uint8_t watchdog_change_enable = _BV(WDTOE);
#else
#error "Unsupported watchdog change-enable bit for this MCU"
#endif

#if defined(WDTCSR)
	WDTCSR = static_cast<uint8_t>(watchdog_change_enable | _BV(WDE));
	WDTCSR = static_cast<uint8_t>(_BV(WDIE) | _BV(WDP3) | _BV(WDP0));
#elif defined(WDTCR)
	WDTCR = static_cast<uint8_t>(watchdog_change_enable | _BV(WDE));
	WDTCR = static_cast<uint8_t>(_BV(WDIE) | _BV(WDP3) | _BV(WDP0));
#else
#error "Unsupported watchdog control register for this MCU"
#endif
}

void disable_watchdog() {
	wdt_disable();
}

void enter_deep_sleep() {
	configure_watchdog_8s_interrupt();
	set_sleep_mode(SLEEP_MODE_PWR_DOWN);
	sleep_enable();
	sleep_cpu();
	sleep_disable();
	disable_watchdog();
}

void clear_receive_buffer() {
	while (sim_serial.available() > 0) {
		sim_serial.read();
	}
}

uint8_t read_response(char* buffer, uint8_t buffer_size, unsigned long timeout_ms) {
	uint8_t position = 0U;
	unsigned long start = millis();

	while (static_cast<unsigned long>(millis() - start) < timeout_ms && position < static_cast<uint8_t>(buffer_size - 1U)) {
		if (sim_serial.available() <= 0) {
			continue;
		}

		int read_value = sim_serial.read();
		if (read_value < 0) {
			continue;
		}

		buffer[position++] = static_cast<char>(read_value);
		buffer[position] = SIM_CHAR_END_OF_STRING;

		if (strstr(buffer, SIM_SMS_RESPONSE_OK_WITH_CRLF) != nullptr || strstr(buffer, SIM_SMS_RESPONSE_ERROR_WITH_CRLF) != nullptr) {
			break;
		}

		if (strstr(buffer, SIM_SMS_RESPONSE_OK_SUFFIX) != nullptr || strstr(buffer, SIM_SMS_RESPONSE_ERROR_SUFFIX) != nullptr) {
			break;
		}
	}

	buffer[position] = SIM_CHAR_END_OF_STRING;
	return position;
}

bool turn_off_sim_led() {
	for (uint8_t attempt = 0U; attempt < SIM_NETLIGHT_MAX_ATTEMPTS; ++attempt) {
		sim_serial.listen();
		clear_receive_buffer();
		delay(SIM_NETLIGHT_PRE_DELAY_MS);
		send_at_cmd(SIM_NETLIGHT_CMD_OFF);
		if (wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_NETLIGHT_TIMEOUT_MS)) {
			return true;
		}
		delay(SIM_NETLIGHT_RETRY_DELAY_MS);
	}

	return false;
}

bool sim_800c_enter_sleep_mode() {
	sim_serial.listen();
	digitalWrite(SIM_SLEEP_PIN, LOW);
	delay(SIM_SLEEP_ENTER_PRE_DELAY_MS);
	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_ENABLE_DTR_SLEEP);
	if (!wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SLEEP_CMD_TIMEOUT_MS)) {
		return false;
	}
	digitalWrite(SIM_SLEEP_PIN, HIGH);
	return true;
}

bool sim_800c_exit_sleep_mode() {
	sim_serial.listen();
	digitalWrite(SIM_SLEEP_PIN, LOW);
	delay(SIM_SLEEP_EXIT_PRE_DELAY_MS);
	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_BASIC_AT);
	return wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SLEEP_CMD_TIMEOUT_MS);
}

int get_sms_count() {
	sim_serial.listen();

	if (!sms_receiver_initialized) {
		setup_sms_receiver();
		if (!sms_receiver_initialized) {
			return SIM_INVALID_SMS_COUNT;
		}
	}

	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_GET_STORAGE_STATUS);

	char response_buffer[SIM_SMS_RESPONSE_BUFFER_SIZE] = {};
	read_response(response_buffer, static_cast<uint8_t>(sizeof(response_buffer)), SIM_SMS_STORAGE_TIMEOUT_MS);

	const char* header = strstr(response_buffer, SIM_SMS_STORAGE_HEADER);
	if (header == nullptr) {
		DEBUG_FAIL("e_cpms_hdr");
		return SIM_INVALID_SMS_COUNT;
	}

	const char* cursor = header;
	while (*cursor != SIM_CHAR_END_OF_STRING && *cursor != ',') {
		++cursor;
	}

	if (*cursor != ',') {
		DEBUG_FAIL("e_cpms_comma");
		return SIM_INVALID_SMS_COUNT;
	}

	++cursor;
	int value = 0;
	bool has_digit = false;
	while (*cursor >= '0' && *cursor <= '9') {
		has_digit = true;
		value = (value * 10) + (*cursor - '0');
		++cursor;
	}

	if (!has_digit) {
		DEBUG_FAIL("e_cpms_digit");
		return SIM_INVALID_SMS_COUNT;
	}

	return value;
}

bool read_sms(uint8_t index, char* message) {
	sim_serial.listen();
	message[0] = SIM_CHAR_END_OF_STRING;
	if (!sms_receiver_initialized) {
		setup_sms_receiver();
		if (!sms_receiver_initialized) {
			return false;
		}
	}

	clear_receive_buffer();
	sim_serial.print(SIM_SMS_CMD_READ_PREFIX);
	if (index >= 100U) {
		sim_serial.write((uint8_t)('0' + (index / 100U)));
		index %= 100U;
		sim_serial.write((uint8_t)('0' + (index / 10U)));
		sim_serial.write((uint8_t)('0' + (index % 10U)));
	}
	else if (index >= 10U) {
		sim_serial.write((uint8_t)('0' + (index / 10U)));
		sim_serial.write((uint8_t)('0' + (index % 10U)));
	}
	else {
		sim_serial.write((uint8_t)('0' + index));
	}
	sim_serial.write(SIM_CHAR_CARRIAGE_RETURN);

	uint8_t state = 0U;
	uint8_t header_pos = 0U;
	uint8_t message_pos = 0U;
	unsigned long start = millis();

	char trailer[6] = {};
	uint8_t trailer_pos = 0U;

	while (static_cast<unsigned long>(millis() - start) < SIM_READ_SMS_TIMEOUT_MS && message_pos < static_cast<uint8_t>(SIM_SMS_MESSAGE_BUFFER_SIZE - 1U)) {
		if (sim_serial.available() <= 0) {
			continue;
		}

		int read_value = sim_serial.read();
		if (read_value < 0) {
			continue;
		}

		char c = static_cast<char>(read_value);

		if (trailer_pos < static_cast<uint8_t>(sizeof(trailer) - 1U)) {
			trailer[trailer_pos++] = c;
		}
		else {
			for (uint8_t i = 0U; i < static_cast<uint8_t>(sizeof(trailer) - 2U); ++i) {
				trailer[i] = trailer[i + 1U];
			}
			trailer[static_cast<uint8_t>(sizeof(trailer) - 2U)] = c;
		}
		trailer[trailer_pos < static_cast<uint8_t>(sizeof(trailer)) ? trailer_pos : static_cast<uint8_t>(sizeof(trailer) - 1U)] = SIM_CHAR_END_OF_STRING;

		if (strstr(trailer, SIM_SMS_LITERAL_ERROR) != nullptr) {
			break;
		}
		if (state >= 2U && strstr(trailer, SIM_SMS_LITERAL_OK) != nullptr) {
			break;
		}

		if (state == 0U) {
			if (c == SIM_SMS_HEADER[header_pos]) {
				++header_pos;
				if (SIM_SMS_HEADER[header_pos] == SIM_CHAR_END_OF_STRING) {
					state = 1U;
				}
			}
			else {
				header_pos = (c == SIM_SMS_HEADER[0]) ? 1U : 0U;
			}

			continue;
		}

		if (state == 1U) {
			if (c == (char)DEBUG_CHAR_LINE_FEED) {
				state = 2U;
			}
			continue;
		}

		if (state == 2U) {
			if (c == (char)SIM_CHAR_CARRIAGE_RETURN || c == (char)DEBUG_CHAR_LINE_FEED) {
				continue;
			}

			message[message_pos++] = c;
			state = 3U;
			continue;
		}

		if (c == (char)SIM_CHAR_CARRIAGE_RETURN || c == (char)DEBUG_CHAR_LINE_FEED) {
			break;
		}

		message[message_pos++] = c;
	}

	message[message_pos] = SIM_CHAR_END_OF_STRING;
	return message_pos > 0U;
}

bool delete_all_sms() {
	sim_serial.listen();
	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_DELETE_ALL);
	return wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SMS_STORAGE_TIMEOUT_MS);
}

bool store_phone_local_number_to_eeprom(const char* phone_local_number) {
	if (phone_local_number[APP_PHONE_LOCAL_NUMBER_LENGTH] != SIM_CHAR_END_OF_STRING) {
		return false;
	}

	for (uint8_t index = 0U; index < APP_PHONE_LOCAL_NUMBER_LENGTH; ++index) {
		const char c = phone_local_number[index];
		if (c < '0' || c > '9') {
			return false;
		}

		eeprom_update_byte(
			reinterpret_cast<uint8_t*>(static_cast<uint16_t>(EEPROM_PHONE_BASE_ADDRESS + index)),
			static_cast<uint8_t>(c));
	}

	eeprom_update_byte(
		reinterpret_cast<uint8_t*>(static_cast<uint16_t>(EEPROM_PHONE_BASE_ADDRESS + APP_PHONE_LOCAL_NUMBER_LENGTH)),
		static_cast<uint8_t>(SIM_CHAR_END_OF_STRING));
	return true;
}

bool call_phone_number_from_eeprom() {
	for (uint8_t index = 0U; index < APP_PHONE_LOCAL_NUMBER_LENGTH; ++index) {
		uint8_t value = eeprom_read_byte(
			reinterpret_cast<const uint8_t*>(static_cast<uint16_t>(EEPROM_PHONE_BASE_ADDRESS + index)));
		char c = static_cast<char>(value);
		if (c < '0' || c > '9') {
			DEBUG_FAIL("e_call_num");
			return false;
		}
	}

	sim_serial.listen();
	clear_receive_buffer();
	sim_serial.write((uint8_t)'A');
	sim_serial.write((uint8_t)'T');
	sim_serial.write((uint8_t)'D');
	sim_serial.write(APP_PHONE_COUNTRY_PREFIX_CHAR_1);
	sim_serial.write(APP_PHONE_COUNTRY_PREFIX_CHAR_2);
	sim_serial.write(APP_PHONE_COUNTRY_PREFIX_CHAR_3);
	for (uint8_t index = 0U; index < APP_PHONE_LOCAL_NUMBER_LENGTH; ++index) {
		uint8_t value = eeprom_read_byte(
			reinterpret_cast<const uint8_t*>(static_cast<uint16_t>(EEPROM_PHONE_BASE_ADDRESS + index)));
		sim_serial.write(value);
	}
	sim_serial.write((uint8_t)';');
	sim_serial.write(SIM_CHAR_CARRIAGE_RETURN);
	if (!wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_CALL_START_TIMEOUT_MS)) {
		DEBUG_FAIL("e_call_ok");
		return false;
	}
	delay(SIM_CALL_POST_DIAL_DELAY_MS);
	DEBUG_LOG("d_call_ok");
	return true;
}

bool set_auto_answer_mode(bool enabled) {
	sim_serial.listen();
	clear_receive_buffer();
	send_at_cmd(enabled ? SIM_SMS_CMD_AUTO_ANSWER_3_RINGS : SIM_SMS_CMD_AUTO_ANSWER_DISABLED);
	return wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SLEEP_CMD_TIMEOUT_MS);
}

void handle_sms_command(const char* message) {
	if (message[0] == SIM_CHAR_END_OF_STRING) {
		DEBUG_FAIL("e_cmd_empty");
		return;
	}

	if (message[0] == SMS_CMD_AUTO_ANSWER_ENABLE) {
		DEBUG_LOG("d_cmd_1");
		if (!set_auto_answer_mode(true)) {
			DEBUG_FAIL("e_ans_on");
			return;
		}
		sleep_mode_enabled = false;
		sim800c_sleep_state = false;
		watchdog_poll_elapsed_ticks = 0U;
		DEBUG_LOG("d_ans_on");
		if (!call_phone_number_from_eeprom()) {
			DEBUG_FAIL("e_call_cmd");
		}
		return;
	}

	if (message[0] == SMS_CMD_AUTO_ANSWER_DISABLE) {
		DEBUG_LOG("d_cmd_2");
		if (!set_auto_answer_mode(false)) {
			DEBUG_FAIL("e_ans_off");
			return;
		}
		sleep_mode_enabled = true;
		watchdog_poll_elapsed_ticks = 0U;
		DEBUG_LOG("d_ans_off");
		if (!call_phone_number_from_eeprom()) {
			DEBUG_FAIL("e_call_cmd");
		}
		return;
	}

	if (message[0] != SMS_CMD_STORE_PHONE) {
		DEBUG_FAIL("e_cmd_unk");
		return;
	}

	DEBUG_LOG("d_cmd_num");
	if (!store_phone_local_number_to_eeprom(message + 1)) {
		DEBUG_FAIL("e_num_save");
		return;
	}
	DEBUG_LOG("d_num_ok");

	if (!call_phone_number_from_eeprom()) {
		DEBUG_FAIL("e_call_cmd");
	}
}

bool setup_sms_receiver() {
	bool is_ok = true;

	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_SET_MEMORY);
	is_ok = is_ok && wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SMS_SETUP_TIMEOUT_MS);
	delay(SIM_SMS_SETUP_STEP_DELAY_MS);

	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_TEXT_MODE);
	is_ok = is_ok && wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SMS_SETUP_TIMEOUT_MS);
	delay(SIM_SMS_SETUP_STEP_DELAY_MS);

	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_CHARSET);
	is_ok = is_ok && wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SMS_SETUP_TIMEOUT_MS);
	delay(SIM_SMS_SETUP_STEP_DELAY_MS);

	clear_receive_buffer();
	send_at_cmd(SIM_SMS_CMD_CNMI);
	is_ok = is_ok && wait_for_pattern(SIM_SMS_LITERAL_OK, SIM_SMS_SETUP_TIMEOUT_MS);
	delay(SIM_SMS_SETUP_STEP_DELAY_MS);

	sms_receiver_initialized = is_ok;
	return is_ok;
}

void setup() {
	pinMode(SIM_SLEEP_PIN, OUTPUT);
	digitalWrite(SIM_SLEEP_PIN, LOW);

	sim_serial.begin(SIM_SERIAL_BAUD_RATE);
#if APP_DEBUG_SERIAL_ENABLED
	debug_serial.begin(DEBUG_SERIAL_BAUD_RATE);
#endif
	delay(SIM_BOOT_DELAY_MS);

	bool is_led_off = turn_off_sim_led();
	if (!is_led_off) {
		DEBUG_FAIL("e_led");
	}

	bool is_sms_ready = setup_sms_receiver();
	if (!is_sms_ready) {
		DEBUG_FAIL("e_sms");
	}

	bool is_sms_deleted = delete_all_sms();
	if (!is_sms_deleted) {
		DEBUG_FAIL("e_del");
	}

	if (!set_auto_answer_mode(false)) {
		DEBUG_FAIL("e_ans_reset");
	}
	sleep_mode_enabled = true;

	if (is_led_off && is_sms_ready && is_sms_deleted) {
		if (!call_phone_number_from_eeprom()) {
			DEBUG_FAIL("e_call_boot");
		}
	}
}

void poll_sms_and_handle_if_any() {
	int sms_count = get_sms_count();
	if (sms_count > 0) {
		char message[SIM_SMS_MESSAGE_BUFFER_SIZE] = {};
		bool is_sms_read = false;
		uint8_t max_index_to_scan = (sms_count > 245) ? 255U : static_cast<uint8_t>(sms_count + 10);

		for (uint8_t index = SIM_SMS_SLOT_FIRST;; ++index) {
			if (read_sms(index, message)) {
				is_sms_read = true;
				break;
			}
			if (index >= max_index_to_scan) {
				break;
			}
		}

		if (is_sms_read) {
			if (!delete_all_sms()) {
				DEBUG_FAIL("e_del_loop");
			}
			handle_sms_command(message);
		}
		else {
			DEBUG_FAIL("e_sms_read");
		}
	}
}

void loop() {
	if (!sleep_mode_enabled) {
		if (sim800c_sleep_state) {
			DEBUG_LOG("d_sleep_out_try");
			if (sim_800c_exit_sleep_mode()) {
				sim800c_sleep_state = false;
				DEBUG_LOG("d_sleep_out_ok");
			}
			else {
				DEBUG_FAIL("e_sleep_out");
				delay(1000);
				return;
			}
		}
		delay(SIM_POST_WAKE_SETTLE_MS);
		poll_sms_and_handle_if_any();
		delay(ACTIVE_POLL_DELAY_MS);
		return;
	}

	watchdog_wake_state = false;

	if (!sim800c_sleep_state) {
		if (sim_800c_enter_sleep_mode()) {
			sim800c_sleep_state = true;
			DEBUG_LOG("d_sleep_in_ok");
		}
		else {
			DEBUG_FAIL("e_sleep_in");
			delay(1000);
			return;
		}
	}

	enter_deep_sleep();

	if (!watchdog_wake_state) {
		return;
	}

	if (watchdog_poll_elapsed_ticks < WATCHDOG_POLL_INTERVAL_TICKS) {
		++watchdog_poll_elapsed_ticks;
	}

	if (watchdog_poll_elapsed_ticks < WATCHDOG_POLL_INTERVAL_TICKS) {
		return;
	}

	watchdog_poll_elapsed_ticks = 0U;

	if (sim800c_sleep_state) {
		DEBUG_LOG("d_sleep_out_try");
		if (sim_800c_exit_sleep_mode()) {
			sim800c_sleep_state = false;
			DEBUG_LOG("d_sleep_out_ok");
		}
		else {
			DEBUG_FAIL("e_sleep_out");
			return;
		}
	}
	delay(SIM_POST_WAKE_SETTLE_MS);
	poll_sms_and_handle_if_any();
}
