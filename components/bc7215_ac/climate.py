import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate
from esphome.const import CONF_ID

CONF_BC7215_UART_NUM = "bc7215_uart_num"
CONF_BC7215_TX_PIN = "bc7215_tx_pin"
CONF_BC7215_RX_PIN = "bc7215_rx_pin"
CONF_BC7215_BUSY_PIN = "bc7215_busy_pin"
CONF_BC7215_MOD_PIN = "bc7215_mod_pin"
CONF_LIBRARY_TEMPERATURE_UNIT = "library_temperature_unit"

UNIT_MAP = {
    "C": "C",
    "CELSIUS": "C",
    "F": "F",
    "FAHRENHEIT": "F",
}


def validate_gpio_number(value):
    """Accept either 17 or GPIO17 in YAML, then store it as an integer."""
    if isinstance(value, int):
        return cv.int_range(min=0, max=48)(value)

    value = cv.string(value).strip().upper()
    if value.startswith("GPIO"):
        value = value[4:]

    try:
        pin = int(value, 10)
    except ValueError as err:
        raise cv.Invalid("GPIO pin must be an integer or a string like GPIO17") from err

    return cv.int_range(min=0, max=48)(pin)


bc7215_ac_ns = cg.esphome_ns.namespace("bc7215_ac")
BC7215ACClimate = bc7215_ac_ns.class_(
    "BC7215ACClimate",
    climate.Climate,
    cg.Component,
)

CONFIG_SCHEMA = (
    climate.climate_schema(BC7215ACClimate)
    .extend(
        {
            cv.Required(CONF_BC7215_UART_NUM): cv.int_range(min=0, max=2),
            cv.Required(CONF_BC7215_TX_PIN): validate_gpio_number,
            cv.Required(CONF_BC7215_RX_PIN): validate_gpio_number,
            cv.Required(CONF_BC7215_BUSY_PIN): validate_gpio_number,
            cv.Required(CONF_BC7215_MOD_PIN): validate_gpio_number,
            # This controls the BC7215 AC protocol library's table/offset selection.
            # ESPHome climate values are still handled as Celsius internally.
            cv.Optional(CONF_LIBRARY_TEMPERATURE_UNIT, default="C"): cv.enum(
                UNIT_MAP, upper=True
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])

    await cg.register_component(var, config)
    await climate.register_climate(var, config)

    cg.add(var.set_uart_num(config[CONF_BC7215_UART_NUM]))
    cg.add(var.set_tx_pin(config[CONF_BC7215_TX_PIN]))
    cg.add(var.set_rx_pin(config[CONF_BC7215_RX_PIN]))
    cg.add(var.set_busy_pin(config[CONF_BC7215_BUSY_PIN]))
    cg.add(var.set_mod_pin(config[CONF_BC7215_MOD_PIN]))
    cg.add(
        var.set_library_unit_celsius(
            config[CONF_LIBRARY_TEMPERATURE_UNIT] == "C"
        )
    )
