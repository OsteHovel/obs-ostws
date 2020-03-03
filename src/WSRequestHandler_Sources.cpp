#include <QString>
#include "Utils.h"

#include "WSRequestHandler.h"

/**
* List filters applied to a source
*
* @param {String} `sourceName` Source name
*
* @return {Array of Objects} `filters` List of filters for the specified source
* @return {String} `filters.*.type` Filter type
* @return {String} `filters.*.name` Filter name
* @return {Object} `filters.*.settings` Filter settings
*
* @api requests
* @name GetSourceFilters
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleGetSourceFilters(WSRequestHandler* req)
{
	if (!req->hasField("sourceName"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSDataArrayAutoRelease filters = obs_data_array_create();
	obs_source_enum_filters(source, [](obs_source_t* parent, obs_source_t* child, void* param)
	{
		OBSDataAutoRelease filter = obs_data_create();
		obs_data_set_string(filter, "type", obs_source_get_id(child));
		obs_data_set_string(filter, "name", obs_source_get_name(child));
		obs_data_set_obj(filter, "settings", obs_source_get_settings(child));
		obs_data_array_push_back((obs_data_array_t*)param, filter);
	}, filters);

	OBSDataAutoRelease response = obs_data_create();
	obs_data_set_array(response, "filters", filters);
	req->SendOKResponse(response);
}

/**
* Add a new filter to a source. Available source types along with their settings properties are available from `GetSourceTypesList`.
*
* @param {String} `sourceName` Name of the source on which the filter is added
* @param {String} `filterName` Name of the new filter
* @param {String} `filterType` Filter type
* @param {Object} `filterSettings` Filter settings
*
* @api requests
* @name AddFilterToSource
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleAddFilterToSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") ||
		!req->hasField("filterType") || !req->hasField("filterSettings"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	const char* filterType = obs_data_get_string(req->data, "filterType");
	OBSDataAutoRelease filterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease existingFilter = obs_source_get_filter_by_name(source, filterName);
	if (existingFilter)
	{
		req->SendErrorResponse("filter name already taken");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_create_private(filterType, filterName, filterSettings);
	if (!filter)
	{
		req->SendErrorResponse("filter creation failed");
		return;
	}
	if (obs_source_get_type(filter) != OBS_SOURCE_TYPE_FILTER)
	{
		req->SendErrorResponse("invalid filter type");
		return;
	}

	obs_source_filter_add(source, filter);

	req->SendOKResponse();
}

/**
* Remove a filter from a source
*
* @param {String} `sourceName` Name of the source from which the specified filter is removed
* @param {String} `filterName` Name of the filter to remove
*
* @api requests
* @name RemoveFilterFromSource
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleRemoveFilterFromSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter)
	{
		req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_source_filter_remove(source, filter);

	req->SendOKResponse();
}

/**
* Move a filter in the chain (absolute index positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {Integer} `newIndex` Desired position of the filter in the chain
*
* @api requests
* @name ReorderSourceFilter
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleReorderSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("newIndex"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	int newIndex = obs_data_get_int(req->data, "newIndex");

	if (newIndex < 0)
	{
		req->SendErrorResponse("invalid index");
		return;
	}

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter)
	{
		req->SendErrorResponse("specified filter doesn't exist");
	}

	struct filterSearch
	{
		int i;
		int filterIndex;
		obs_source_t* filter;
	};
	struct filterSearch ctx = {0, 0, filter};
	obs_source_enum_filters(source, [](obs_source_t* parent, obs_source_t* child, void* param)
	{
		struct filterSearch* ctx = (struct filterSearch*)param;
		if (child == ctx->filter)
		{
			ctx->filterIndex = ctx->i;
		}
		ctx->i++;
	}, &ctx);

	int lastFilterIndex = ctx.i + 1;
	if (newIndex > lastFilterIndex)
	{
		req->SendErrorResponse("index out of bounds");
		return;
	}

	int currentIndex = ctx.filterIndex;
	if (newIndex > currentIndex)
	{
		int downSteps = newIndex - currentIndex;
		for (int i = 0; i < downSteps; i++)
		{
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_DOWN);
		}
	}
	else if (newIndex < currentIndex)
	{
		int upSteps = currentIndex - newIndex;
		for (int i = 0; i < upSteps; i++)
		{
			obs_source_filter_set_order(source, filter, OBS_ORDER_MOVE_UP);
		}
	}

	req->SendOKResponse();
}

/**
* Move a filter in the chain (relative positioning)
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reorder
* @param {String} `movementType` How to move the filter around in the source's filter chain. Either "up", "down", "top" or "bottom".
*
* @api requests
* @name MoveSourceFilter
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleMoveSourceFilter(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("movementType"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	QString movementType(obs_data_get_string(req->data, "movementType"));

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter)
	{
		req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_order_movement movement;
	if (movementType == "up")
	{
		movement = OBS_ORDER_MOVE_UP;
	}
	else if (movementType == "down")
	{
		movement = OBS_ORDER_MOVE_DOWN;
	}
	else if (movementType == "top")
	{
		movement = OBS_ORDER_MOVE_TOP;
	}
	else if (movementType == "bottom")
	{
		movement = OBS_ORDER_MOVE_BOTTOM;
	}
	else
	{
		req->SendErrorResponse("invalid value for movementType: must be either 'up', 'down', 'top' or 'bottom'.");
		return;
	}

	obs_source_filter_set_order(source, filter, movement);

	req->SendOKResponse();
}

/**
* Update settings of a filter
*
* @param {String} `sourceName` Name of the source to which the filter belongs
* @param {String} `filterName` Name of the filter to reconfigure
* @param {Object} `filterSettings` New settings. These will be merged to the current filter settings.
*
* @api requests
* @name SetSourceFilterSettings
* @category sources
* @since unreleased
*/
void WSRequestHandler::HandleSetSourceFilterSettings(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("filterSettings"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	OBSDataAutoRelease newFilterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter)
	{
		req->SendErrorResponse("specified filter doesn't exist");
	}

	OBSDataAutoRelease settings = obs_source_get_settings(filter);
	obs_data_apply(settings, newFilterSettings);
	obs_source_update(filter, settings);

	req->SendOKResponse();
}

void WSRequestHandler::HandleSetSourceFilterVisibility(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("filterName") || !req->hasField("visible"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	const char* filterName = obs_data_get_string(req->data, "filterName");
	OBSDataAutoRelease newFilterSettings = obs_data_get_obj(req->data, "filterSettings");

	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}

	OBSSourceAutoRelease filter = obs_source_get_filter_by_name(source, filterName);
	if (!filter)
	{
		req->SendErrorResponse("specified filter doesn't exist");
	}

	obs_source_set_enabled(filter, obs_data_get_bool(req->data, "visible"));
	req->SendOKResponse();
}

void WSRequestHandler::HandleTriggerHotkeyOnSource(WSRequestHandler* req)
{
	if (!req->hasField("sourceName") || !req->hasField("hotkeyName"))
	{
		req->SendErrorResponse("missing request parameters");
		return;
	}

	const char* sourceName = obs_data_get_string(req->data, "sourceName");
	OBSSourceAutoRelease source = obs_get_source_by_name(sourceName);
	if (!source)
	{
		req->SendErrorResponse("specified source doesn't exist");
		return;
	}


	auto hotkeyName = obs_data_get_string(req->data, "hotkeyName");
	obs_hotkey_id hotkey_id = 0;
	auto data = std::make_tuple(hotkeyName, &source, &hotkey_id);
	using data_t = decltype(data);

	obs_enum_hotkeys([](void* data, obs_hotkey_id id, obs_hotkey_t* key)
	{
		if (obs_hotkey_get_registerer_type(key) != OBS_HOTKEY_REGISTERER_SOURCE)
		{
			return true;
		}
		data_t& d = *static_cast<data_t*>(data);

		const char* hotkey_get_name = obs_hotkey_get_name(key);
		blog(LOG_INFO, "Hotkey: %s", hotkey_get_name);
		if (strcmp(std::get<0>(d), hotkey_get_name) != 0)
		{
			return true;
		}

		auto weak_source = static_cast<obs_weak_source_t*>(obs_hotkey_get_registerer(key));
		auto source = OBSGetStrongRef(weak_source);

		if (source != *std::get<1>(d))
		{
			return true;
		}

		*std::get<2>(d) = id;
		return false;
	}, &data);

	blog(LOG_INFO, "DOne, got hotkey? %d",hotkey_id);
	if (hotkey_id == 0)
	{
		blog(LOG_INFO, "Hotkey was not found");
		req->SendErrorResponse("Hotkey was not found");
		return;
	}

	obs_hotkey_trigger_routed_callback(hotkey_id, true);
	obs_hotkey_trigger_routed_callback(hotkey_id, false);

	blog(LOG_INFO, "and where? %d  %d", hotkey_id);

	req->SendOKResponse();
}
