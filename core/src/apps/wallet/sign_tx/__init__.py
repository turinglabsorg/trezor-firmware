from trezor import utils
from trezor.messages.RequestType import TXFINISHED
from trezor.messages.TxAck import TxAck
from trezor.messages.TxRequest import TxRequest

from apps.common import paths
from apps.wallet.sign_tx import helpers, layout, progress
from apps.wallet.sign_tx.bitcoin import signing as bitcoin_signing
from apps.wallet.sign_tx.bitcoin_like import signing as bitcoin_like_signing


async def sign_tx(ctx, msg, keychain):

    if not msg.coin_name or msg.coin_name in ("Bitcoin", "Testnet"):  # TODO testnet?
        print("A")
        signer = bitcoin_signing.sign_tx(msg, keychain)
    else:
        print("B")
        signer = bitcoin_like_signing.sign_tx(msg, keychain)
    print(msg.coin_name)

    res = None
    while True:
        # try:
        req = signer.send(res)
        # except (
        #     # signing.SigningError,  # TODO
        #     multisig.MultisigError,
        #     addresses.AddressError,
        #     scripts.ScriptsError,
        #     segwit_bip143.Bip143Error,
        # ) as e:
        #     raise wire.Error(*e.args)  # TODO this looks dangerous
        if isinstance(req, TxRequest):
            if req.request_type == TXFINISHED:
                break
            res = await ctx.call(req, TxAck)
        elif isinstance(req, helpers.UiConfirmOutput):
            mods = utils.unimport_begin()
            res = await layout.confirm_output(ctx, req.output, req.coin)
            utils.unimport_end(mods)
            progress.report_init()
        elif isinstance(req, helpers.UiConfirmTotal):
            mods = utils.unimport_begin()
            res = await layout.confirm_total(ctx, req.spending, req.fee, req.coin)
            utils.unimport_end(mods)
            progress.report_init()
        elif isinstance(req, helpers.UiConfirmFeeOverThreshold):
            mods = utils.unimport_begin()
            res = await layout.confirm_feeoverthreshold(ctx, req.fee, req.coin)
            utils.unimport_end(mods)
            progress.report_init()
        elif isinstance(req, helpers.UiConfirmNonDefaultLocktime):
            mods = utils.unimport_begin()
            res = await layout.confirm_nondefault_locktime(ctx, req.lock_time)
            utils.unimport_end(mods)
            progress.report_init()
        elif isinstance(req, helpers.UiConfirmForeignAddress):
            mods = utils.unimport_begin()
            res = await paths.show_path_warning(ctx, req.address_n)
            utils.unimport_end(mods)
            progress.report_init()
        else:
            raise TypeError("Invalid signing instruction")
    return req
