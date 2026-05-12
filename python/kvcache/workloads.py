from __future__ import annotations

import random
from dataclasses import dataclass, field
from typing import Dict, Iterator, Protocol, Tuple


@dataclass(frozen=True)
class Request:
    request_id: int
    tokens: Tuple[int, ...]
    metadata: Dict[str, object] = field(default_factory=dict, hash=False, compare=False)

    @property
    def num_tokens(self) -> int:
        return len(self.tokens)


class Workload(Protocol):
    def generate(self) -> Iterator[Request]: ...
    def describe(self) -> Dict[str, object]: ...


class SharedSystemPromptWorkload:
    def __init__(
        self,
        *,
        block_size: int = 16,
        prefix_blocks: int = 4,
        suffix_blocks: int = 2,
        num_requests: int = 200,
        seed: int = 42,
    ) -> None:
        self._block_size = block_size
        self._prefix_blocks = prefix_blocks
        self._suffix_blocks = suffix_blocks
        self._num_requests = num_requests
        self._seed = seed
        self._prefix = tuple(range(1, prefix_blocks * block_size + 1))

    def generate(self) -> Iterator[Request]:
        rng = random.Random(self._seed)
        for req_id in range(self._num_requests):
            suffix = tuple(
                rng.randint(10_000, 99_999)
                for _ in range(self._suffix_blocks * self._block_size)
            )
            yield Request(
                request_id=req_id,
                tokens=self._prefix + suffix,
                metadata={"workload": "shared_system_prompt"},
            )

    def describe(self) -> Dict[str, object]:
        return {
            "kind": "shared_system_prompt",
            "num_requests": self._num_requests,
            "block_size": self._block_size,
            "prefix_blocks": self._prefix_blocks,
            "suffix_blocks": self._suffix_blocks,
            "seed": self._seed,
        }


class FewShotFamiliesWorkload:
    """
    Requests belong to one of K families. All requests in a family share the same
    prefix token sequence. Suffixes are unique per request. This workload is designed
    to expose routing policy effects: hash routing pins each family to one worker,
    while random routing scatters them and forces cross-worker fetches.
    """

    def __init__(
        self,
        *,
        block_size: int = 16,
        num_families: int = 4,
        prefix_blocks: int = 3,
        suffix_blocks: int = 1,
        num_requests: int = 400,
        seed: int = 42,
    ) -> None:
        self._block_size = block_size
        self._num_families = num_families
        self._prefix_blocks = prefix_blocks
        self._suffix_blocks = suffix_blocks
        self._num_requests = num_requests
        self._seed = seed

    def _family_prefix(self, family_id: int) -> Tuple[int, ...]:
        base = (family_id + 1) * 1000
        return tuple(base + i for i in range(self._prefix_blocks * self._block_size))

    def generate(self) -> Iterator[Request]:
        rng = random.Random(self._seed)
        for req_id in range(self._num_requests):
            family_id = req_id % self._num_families
            prefix = self._family_prefix(family_id)
            suffix = tuple(
                rng.randint(10_000, 99_999)
                for _ in range(self._suffix_blocks * self._block_size)
            )
            yield Request(
                request_id=req_id,
                tokens=prefix + suffix,
                metadata={"workload": "few_shot_families", "family_id": family_id},
            )

    def describe(self) -> Dict[str, object]:
        return {
            "kind": "few_shot_families",
            "num_requests": self._num_requests,
            "num_families": self._num_families,
            "block_size": self._block_size,
            "prefix_blocks": self._prefix_blocks,
            "suffix_blocks": self._suffix_blocks,
            "seed": self._seed,
        }


class AdversarialChurnWorkload:
    """
    Every request has a fully random token sequence. No prefix sharing occurs.
    Designed to stress eviction policies under maximum cache pressure: every admitted
    block is equally valueless, so eviction policy differences are minimal and hit
    rate stays near zero. Useful as the baseline comparison for prefix-reuse demos.
    """

    def __init__(
        self,
        *,
        block_size: int = 16,
        request_blocks: int = 6,
        num_requests: int = 300,
        seed: int = 42,
    ) -> None:
        self._block_size = block_size
        self._request_blocks = request_blocks
        self._num_requests = num_requests
        self._seed = seed

    def generate(self) -> Iterator[Request]:
        rng = random.Random(self._seed)
        for req_id in range(self._num_requests):
            tokens = tuple(
                rng.randint(1, 999_999)
                for _ in range(self._request_blocks * self._block_size)
            )
            yield Request(
                request_id=req_id,
                tokens=tokens,
                metadata={"workload": "adversarial_churn"},
            )

    def describe(self) -> Dict[str, object]:
        return {
            "kind": "adversarial_churn",
            "num_requests": self._num_requests,
            "block_size": self._block_size,
            "request_blocks": self._request_blocks,
            "seed": self._seed,
        }


class MultiTurnChatWorkload:
    """
    Simulates growing conversation histories. Each turn appends new tokens to the
    accumulated history, so later turns in a conversation reuse all earlier KV blocks.
    Reuse grows across turns, and memory pressure rises as histories become long.
    Both admission and eviction policy matter: evicting an early turn block makes
    all later turns in that conversation cold again.
    """

    def __init__(
        self,
        *,
        block_size: int = 16,
        num_conversations: int = 10,
        turns_per_conversation: int = 6,
        blocks_per_turn: int = 2,
        seed: int = 42,
    ) -> None:
        self._block_size = block_size
        self._num_conversations = num_conversations
        self._turns_per_conversation = turns_per_conversation
        self._blocks_per_turn = blocks_per_turn
        self._seed = seed

    def generate(self) -> Iterator[Request]:
        rng = random.Random(self._seed)
        req_id = 0
        for conv_id in range(self._num_conversations):
            history: Tuple[int, ...] = ()
            for turn in range(self._turns_per_conversation):
                new_tokens = tuple(
                    rng.randint(10_000, 99_999)
                    for _ in range(self._blocks_per_turn * self._block_size)
                )
                history = history + new_tokens
                yield Request(
                    request_id=req_id,
                    tokens=history,
                    metadata={
                        "workload": "multi_turn_chat",
                        "conversation_id": conv_id,
                        "turn": turn,
                    },
                )
                req_id += 1

    def describe(self) -> Dict[str, object]:
        return {
            "kind": "multi_turn_chat",
            "num_conversations": self._num_conversations,
            "turns_per_conversation": self._turns_per_conversation,
            "blocks_per_turn": self._blocks_per_turn,
            "block_size": self._block_size,
            "num_requests": self._num_conversations * self._turns_per_conversation,
            "seed": self._seed,
        }
